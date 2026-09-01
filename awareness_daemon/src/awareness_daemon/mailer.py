import os
import smtplib
from email.message import EmailMessage


class Mailer:
    """Thin SMTP wrapper. Opens one connection per batch and isolates
    per-recipient failures so a single bad address cannot abort a run."""

    def __init__(self, logger, host, port, username, password, from_addr, use_tls=True):
        self.logger = logger
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.from_addr = from_addr
        self.use_tls = use_tls

    @classmethod
    def from_env(cls, logger):
        return cls(
            logger=logger,
            host=os.environ.get("EPSA_SMTP_HOST", "localhost"),
            port=int(os.environ.get("EPSA_SMTP_PORT", "587")),
            username=os.environ.get("EPSA_SMTP_USER", ""),
            password=os.environ.get("EPSA_SMTP_PASSWORD", ""),
            from_addr=os.environ.get("EPSA_MAIL_FROM", "eps-awareness@example.org"),
            use_tls=os.environ.get("EPSA_SMTP_TLS", "true").lower() == "true",
        )

    def _connect(self):
        smtp = smtplib.SMTP(self.host, self.port, timeout=30)
        if self.use_tls:
            smtp.starttls()
        if self.username:
            smtp.login(self.username, self.password)
        return smtp

    def send_batch(self, messages):
        """messages: iterable of (to_addr, subject, body).

        Returns the number of messages successfully handed to the server.
        """
        messages = list(messages)
        sent = 0
        try:
            with self._connect() as smtp:
                for to_addr, subject, body in messages:
                    try:
                        msg = EmailMessage()
                        msg["From"] = self.from_addr
                        msg["To"] = to_addr
                        msg["Subject"] = subject
                        msg.set_content(body)
                        smtp.send_message(msg)
                        sent += 1
                    except Exception:
                        self.logger.exception(f"Failed to send email to {to_addr}")
        except Exception:
            self.logger.exception("SMTP connection failed; skipping this notification run")
        return sent
