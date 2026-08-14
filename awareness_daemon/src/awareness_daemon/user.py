
class EPSAUser:
    def __init__(self, user_id: str, username: str, email: str):
        self.user_id = user_id
        self.username = username
        self.email = email

    def __repr__(self):
        return f"EPSAUser(user_id={self.user_id}, username={self.username}, email={self.email})"