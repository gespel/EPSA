import threading

# The daemon performs its periodic work once every 7 days.
#SEVEN_DAYS_SECONDS = 7 * 24 * 60 * 60
SEVEN_DAYS_SECONDS = 3

class AwarenessDaemon:
    def __init__(self, logger, db_handler, interval_seconds=SEVEN_DAYS_SECONDS):
        self.running = False
        self.logger = logger
        self.db_handler = db_handler
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
        users = self.db_handler.get_all_users()
        for user in users:
            self.logger.info(
                f"User {user.username} (ID: {user.user_id}) has total energy consumption: {user.total_energy_kwh} kWh"
            )
