from flask import Flask
import logging
import db

app = Flask(__name__)
logger = logging.getLogger("awareness_daemon")
logger.setLevel(logging.DEBUG)

handler = logging.StreamHandler()
handler.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s|%(name)s|%(levelname)s: %(message)s')
handler.setFormatter(formatter)

logger.addHandler(handler)

@app.route("/")
def index():
    logger.info("Index route accessed")

    db_handler = db.DatabaseHandler(
        host="localhost",
        database="eps",
        user="eps",
        password="eps"
    )

    username = "root"
    user_id = db_handler.get_userid_by_username(username)

    print(f"UserId of {username} is {user_id}")

    return "Awareness Daemon is running."

def main():
    logger.info("Starting Awareness Daemon WSGI application")
    app.run(host="0.0.0.0", port=5000)

if __name__ == "__main__":
    main()
