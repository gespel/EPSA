
class EPSAUser:
    def __init__(self, user_id: str, username: str, email: str):
        self.user_id = user_id
        self.username = username
        self.email = email
        self.total_energy_kwh = 0.0
        self.job_count = 0
        self.jobs_with_energy = []

    def __repr__(self):
        return f"EPSAUser(user_id={self.user_id}, username={self.username}, email={self.email})"

    def add_energy(self, energy_kwh: float):
        self.total_energy_kwh += energy_kwh

    def get_total_energy(self) -> float:
        return self.total_energy_kwh

    def increment_job_count(self):
        self.job_count += 1

    def set_jobs_with_energy(self, jobs: list):
        self.jobs_with_energy = jobs
