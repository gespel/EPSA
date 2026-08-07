from flask import Flask
import logging

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
    return "Awareness Daemon is running."

def main():
    logger.info("Starting Awareness Daemon WSGI application")
    app.run(host="0.0.0.0", port=5000)

if __name__ == "__main__":
    main()
