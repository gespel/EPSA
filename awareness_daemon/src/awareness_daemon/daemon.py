import threading

# The daemon performs its periodic work once every 7 days.
#SEVEN_DAYS_SECONDS = 7 * 24 * 60 * 60
SEVEN_DAYS_SECONDS = 3

class AwarenessDaemon:
    def __init__(self, logger, db_handler, mailer, comparator, interval_seconds=SEVEN_DAYS_SECONDS):
        self.running = False
        self.logger = logger
        self.db_handler = db_handler
        self.mailer = mailer
        self.comparator = comparator
        self.interval_seconds = interval_seconds
        self._wakeup = threading.Event()

    def start(self):
        self.running = True
        self._wakeup.clear()
        self.logger.info("Awareness Daemon started.")

    def stop(self):
        self.running = False
        self._wakeup.set()
        self.logger.info("Awareness Daemon stopped.")

    def run(self):
        while self.running:
            self._tick()
            self._wakeup.wait(self.interval_seconds)

    def _tick(self):
        self.logger.info("Awareness Daemon tick: Performing periodic tasks...")
        self.send_email_notifications()

    def send_email_notifications(self):
        users = self.db_handler.get_all_users()
        messages = [
            self._build_message(user)
            for user in users
            if user.total_energy_kwh > 0 and user.email
        ]
        sent = self.mailer.send_batch(messages)
        self.logger.info(f"Sent {sent}/{len(messages)} awareness emails")

    def _build_message(self, user):
        device_name, device_time = self.comparator.compare_consumption(user.total_energy_kwh)
        body = (
            f"Hi {user.username},\n\n"
            f"Your total energy consumption over the past week is "
            f"{user.total_energy_kwh:.4f} kWh.\n\n"
            f"That is roughly equivalent to running a {device_name} for {device_time}.\n\n"
            f"Cheers,\n"
            f"The EPSA Awareness Daemon\n"
        )
        return (user.email, "Your energy consumption this week", body)
