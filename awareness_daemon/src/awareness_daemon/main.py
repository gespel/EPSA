from flask import Flask, render_template, abort
import logging
import os
import db
import comparator

app = Flask(__name__)
logger = logging.getLogger("awareness_daemon")
logger.setLevel(logging.DEBUG)

handler = logging.StreamHandler()
handler.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s|%(name)s|%(levelname)s: %(message)s')
handler.setFormatter(formatter)

logger.addHandler(handler)

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


    comp = comparator.Comparator()
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

    db_handler = get_db_handler()
    username = db_handler.get_username_by_userid(userid)
    if username is None:
        abort(404)

    jobs = db_handler.get_jobs_with_energy_for_user(userid)
    total_kwh = sum(j["total_energy_kwh"] or 0 for j in jobs)

    chart_labels = [f"Job {j['jobid']}" for j in jobs][:15]
    chart_values = [round(float(j["total_energy_kwh"] or 0), 4) for j in jobs][:15]

    comp = comparator.Comparator()
    compare_device_name, compare_device_time = comp.compare_consumption(total_kwh)

    return render_template(
        "user_detail.html",
        userid=userid,
        username=username,
        jobs=jobs,
        total_kwh=total_kwh,
        chart_labels=chart_labels,
        chart_values=chart_values,
        compare_device_name=compare_device_name,
        compare_device_time=compare_device_time,
    )


def main():
    logger.info("Starting Awareness Daemon WSGI application")
    app.run(host="0.0.0.0", port=5000)

if __name__ == "__main__":
    main()
