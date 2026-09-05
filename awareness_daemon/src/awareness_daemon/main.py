from flask import Flask, render_template, abort
import logging
import os
import db
import comparator
import daemon
import mailer
import time
from threading import Thread
import yaml

DAEMON_RESTART_DELAY_SECONDS = 60

LOG_FORMAT = "%(asctime)s|%(name)s|%(levelname)s: %(message)s"


def configure_logging(level=logging.DEBUG):
    log = logging.getLogger("awareness_daemon")
    log.setLevel(level)
    if not log.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(logging.Formatter(LOG_FORMAT))
        log.addHandler(handler)
    return log


app = Flask(__name__)
logger = configure_logging(level=logging.INFO)
comp = comparator.Comparator(logger=logger)


def load_config():
    config_path = os.path.join("awareness-daemon-config.yaml")
    if not os.path.exists(config_path):
        logger.warning(f"Configuration file {config_path} not found. Using default settings.")
        return {}

    with open(config_path, "r") as f:
        try:
            config = yaml.safe_load(f)
            logger.info(f"Configuration loaded from {config_path}.")
            return config
        except yaml.YAMLError as e:
            logger.error(f"Error parsing configuration file: {e}")
            return {}


config = load_config()
INSTANCE_NAME = config.get("server", {}).get("instance_name", "EPSA Awareness Dashboard")


@app.context_processor
def inject_instance_name():
    return {"instance_name": INSTANCE_NAME}

# Devcontainer default (see .devcontainer/scripts/init-postgres.sql); override
# with EPS_DB_CONN_STR to point at a real cluster, e.g. via SSH port-forward:
# EPS_DB_CONN_STR="host=localhost port=5432 dbname=perfacct_eps user=eps_writer password=..."
DEFAULT_DSN = "host=localhost port=5432 dbname=eps user=eps password=eps"


def get_db_handler():
    return db.DatabaseHandler(os.environ.get("EPS_DB_CONN_STR", DEFAULT_DSN))


@app.route("/")
def index():
    logger.info("Dashboard index route accessed")

    db_handler = get_db_handler()
    leaderboard = db_handler.get_energy_leaderboard()

    total_kwh = sum(u["total_energy_kwh"] or 0 for u in leaderboard)
    total_jobs = sum(u["job_count"] or 0 for u in leaderboard)
    active_users = len([u for u in leaderboard if (u["total_energy_kwh"] or 0) > 0])

    chart_labels = [u["username"] or f"uid:{u['userid']}" for u in leaderboard[:10]]
    chart_values = [round(float(u["total_energy_kwh"] or 0), 4) for u in leaderboard[:10]]

    compare_device_name, compare_device_time = comp.compare_consumption(total_kwh)

    return render_template(
        "dashboard.html",
        leaderboard=leaderboard,
        total_kwh=total_kwh,
        total_jobs=total_jobs,
        active_users=active_users,
        chart_labels=chart_labels,
        chart_values=chart_values,
        compare_device_name=compare_device_name,
        compare_device_time=compare_device_time,
    )


@app.route("/user/<int:userid>")
def user_detail(userid):
    logger.info(f"User detail route accessed for userid={userid}")
    u = get_db_handler().get_user_by_userid(userid)
    if u is not None:
        logger.info(f"Retrieved user: {u}")
    else:
        logger.warning(f"No user found with userid={userid}")
        abort(404)

    chart_labels = [f"Job {j['jobid']}" for j in u.jobs_with_energy][:15]
    chart_values = [round(float(j["total_energy_kwh"] or 0), 4) for j in u.jobs_with_energy][:15]
    compare_device_name, compare_device_time = comp.compare_consumption(u.total_energy_kwh)

    return render_template(
        "user_detail.html",
        userid=u.user_id,
        username=u.username,
        jobs=u.jobs_with_energy,
        total_kwh=u.total_energy_kwh,
        chart_labels=chart_labels,
        chart_values=chart_values,
        compare_device_name=compare_device_name,
        compare_device_time=compare_device_time,
    )


def _supervise_daemon(d):
    while True:
        worker = Thread(target=d.run, name="awareness-daemon", daemon=True)
        worker.start()
        worker.join()

        if not d.running:
            logger.info("Awareness Daemon worker exited after stop(); supervisor stopping.")
            return

        logger.error(
            "Awareness Daemon worker died unexpectedly; restarting in %ss.",
            DAEMON_RESTART_DELAY_SECONDS,
        )
        time.sleep(DAEMON_RESTART_DELAY_SECONDS)

def main():
    logger.info("Starting Awareness Daemon WSGI application")

    host = config.get("server", {}).get("host", {}).get("address", "0.0.0.0")
    port = config.get("server", {}).get("host", {}).get("port", 5000)

    d = daemon.AwarenessDaemon(
        logger=logger,
        config=config,
        db_handler=get_db_handler(),
        mailer=mailer.Mailer(logger),
        comparator=comp,
    )
    d.start()
    Thread(target=_supervise_daemon, args=(d,), name="awareness-daemon-supervisor", daemon=True).start()

    app.run(host=host, port=port)

if __name__ == "__main__":
    main()
