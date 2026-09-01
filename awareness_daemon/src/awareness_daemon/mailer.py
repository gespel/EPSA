import os
import smtplib
from dataclasses import dataclass
from email.message import EmailMessage


@dataclass
class SmtpConfig:
    host: str = "localhost"
    port: int = 587
    username: str = ""          # empty -> no authentication
    password: str = ""
    from_addr: str = "eps-awareness@example.org"
    use_tls: bool = True


def smtp_config_from_env():
    defaults = SmtpConfig()
    return SmtpConfig(
        host=os.environ.get("EPSA_SMTP_HOST", defaults.host),
        port=int(os.environ.get("EPSA_SMTP_PORT", defaults.port)),
        username=os.environ.get("EPSA_SMTP_USER", defaults.username),
        password=os.environ.get("EPSA_SMTP_PASSWORD", defaults.password),
        from_addr=os.environ.get("EPSA_MAIL_FROM", defaults.from_addr),
        use_tls=os.environ.get("EPSA_SMTP_TLS", "true").lower() == "true",
    )


class Mailer:
    def __init__(self, logger, config=None):
        self.logger = logger
        self.config = config or smtp_config_from_env()

    def send_batch(self, messages):
        messages = list(messages)
        if not messages:
            return 0

        try:
            connection = self._connect()
        except Exception:
            self.logger.exception("Could not connect to SMTP server! skipping this run")
            return 0

        sent = 0
        with connection:
            for to_addr, subject, body in messages:
                if self._send_one(connection, to_addr, subject, body):
                    sent += 1
        return sent

    def _connect(self):
        connection = smtplib.SMTP(self.config.host, self.config.port, timeout=30)
        if self.config.use_tls:
            connection.starttls()
        if self.config.username:
            connection.login(self.config.username, self.config.password)
        return connection

    def _send_one(self, connection, to_addr, subject, body):
        try:
            message = EmailMessage()
            message["From"] = self.config.from_addr
            message["To"] = to_addr
            message["Subject"] = subject
            message.set_content(body)
            connection.send_message(message)
            return True
        except Exception:
            self.logger.exception(f"Could not send e-mail to {to_addr}")
            return False
