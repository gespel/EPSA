"""
Lokaler End-to-End-Testlauf des prep_eps-Plugins (siehe EPSA.md,
"Ergebnisse in der DB prüfen"): Testjob absetzen, auf Abschluss warten,
allocations/executions/measurements in der EPS-DB prüfen.

Voraussetzung: slurmctld/slurmd laufen mit installiertem prep_eps-Plugin,
EPS_DB_CONN_STR ist gesetzt (oder wird aus dem laufenden slurmctld übernommen).
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import psycopg

NODES = int(os.environ.get("NODES", 1))
CORES = int(os.environ.get("CORES", 1))
DURATION = int(os.environ.get("DURATION_SECONDS", 40))
PARTITION = os.environ.get("PARTITION", "")
POLL_INTERVAL = int(os.environ.get("POLL_INTERVAL", 2))
TIMEOUT = int(os.environ.get("TIMEOUT", 1800))

SBATCH_SCRIPT = f"""#!/bin/bash
#SBATCH -N{NODES} -n{NODES} -c{CORES}
{f"#SBATCH -p{PARTITION}" if PARTITION else ""}
end=$((SECONDS + {DURATION}))
for _ in $(seq {CORES}); do
    ( sum=0; i=0; while [ $SECONDS -lt $end ]; do i=$((i + 1)); sum=$((sum + i * i)); done ) &
done
wait
echo "Last auf {CORES} Kern(en) für {DURATION}s abgeschlossen"
"""


def log(msg: str) -> None:
    print(f"[test] {msg}", flush=True)


def fail(msg: str) -> None:
    sys.exit(f"[test] FEHLER: {msg}")


def db_conn_str() -> str:
    if conn := os.environ.get("EPS_DB_CONN_STR"):
        return conn

    # Auf der lokalen Entwicklermaschine läuft slurmctld bereits mit gesetzter
    # EPS_DB_CONN_STR. Statt das DB-Passwort hier zu hinterlegen, übernehmen
    # wir es aus dem environ des laufenden Daemons.
    pids = subprocess.run(
        ["pgrep", "-x", "slurmctld"], capture_output=True, text=True
    ).stdout.split()
    if pids:
        environ = subprocess.run(
            ["sudo", "-n", "cat", f"/proc/{pids[0]}/environ"], capture_output=True
        ).stdout
        for entry in environ.split(b"\0"):
            if entry.startswith(b"EPS_DB_CONN_STR="):
                log(f"EPS_DB_CONN_STR aus laufendem slurmctld (PID {pids[0]}) übernommen.")
                return entry.split(b"=", 1)[1].decode()

    fail("EPS_DB_CONN_STR ist nicht gesetzt und konnte nicht aus slurmctld ermittelt werden")


def submit_job() -> str:
    log(f"Reiche Testjob ein (Nodes={NODES}, Kerne={CORES}, Rechenlast={DURATION}s)...")
    with tempfile.NamedTemporaryFile("w", suffix=".sbatch") as script:
        script.write(SBATCH_SCRIPT)
        script.flush()
        out = subprocess.run(
            ["sbatch", script.name], capture_output=True, text=True, check=True
        ).stdout

    match = re.search(r"\d+$", out.strip())
    if not match:
        fail(f"Konnte JobID nicht aus sbatch-Ausgabe lesen: {out.strip()}")
    return match.group()


def wait_for_completion(jobid: str) -> None:
    log(f"Job {jobid} eingereicht, warte auf Abschluss (Timeout {TIMEOUT}s)...")
    for _ in range(0, TIMEOUT, POLL_INTERVAL):
        out = subprocess.run(
            ["squeue", "-h", "-j", jobid], capture_output=True, text=True
        ).stdout
        if not out.strip():
            log(f"Job {jobid} abgeschlossen.")
            return
        time.sleep(POLL_INTERVAL)
    fail(f"Job {jobid} ist nach {TIMEOUT}s nicht abgeschlossen (Timeout)")


def check_db(jobid: str) -> bool:
    log(f"Prüfe Datenbankeinträge für Job {jobid}...")

    with psycopg.connect(db_conn_str()) as conn, conn.cursor() as cur:
        allocations = cur.execute(
            "SELECT count(*) FROM allocations WHERE jobid = %s", [jobid]
        ).fetchone()[0]
        executions = cur.execute(
            "SELECT count(*) FROM executions WHERE jobid = %s", [jobid]
        ).fetchone()[0]
        measurements = cur.execute(
            "SELECT device_name, device_type, e1 - e0 AS energy_delta, utilization "
            "FROM measurements m JOIN executions e ON m.exec_id = e.id "
            "WHERE e.jobid = %s ORDER BY m.id",
            [jobid],
        ).fetchall()

    bad_deltas = sum(1 for _, _, delta, _ in measurements if delta <= 0)
    total_energy = sum(
        delta * util / 100
        for name, _, delta, util in measurements
        if "package" in name
    )

    log(f"allocations={allocations} executions={executions} "
        f"measurements={len(measurements)} measurements_mit_e1<=e0={bad_deltas}")
    log(f"Gemessener Energieverbrauch für Job {jobid}:")
    for name, dtype, delta, util in measurements:
        attributed = delta * util / 100
        log(f"  {name} ({dtype}): {attributed:.0f} µJ (Socket: {delta} µJ, Auslastung {util}%)")
    log(f"Gesamtenergie (zugerechnet) für Job {jobid}: {total_energy:.0f} µJ")
    log(f"Gesamtenergie in kWh: {total_energy / 3.6e12:.9f}")

    errors = []
    if allocations < 1:
        errors.append("kein Eintrag in allocations")
    if executions < 1:
        errors.append("kein Eintrag in executions")
    if len(measurements) < 2:
        errors.append("weniger als 2 Einträge in measurements")
    if bad_deltas:
        errors.append(f"{bad_deltas} measurement(s) mit e1 <= e0")

    for error in errors:
        log(f"FEHLER: {error}")
    return not errors


def main() -> None:
    for cmd in ("sbatch", "squeue"):
        if not shutil.which(cmd):
            fail(f"{cmd} nicht gefunden (Slurm installiert?)")

    jobid = submit_job()
    wait_for_completion(jobid)
    time.sleep(2)  # slurmd-Epilog Zeit geben, die Messwerte fertig zu schreiben

    if not check_db(jobid):
        fail(f"Testlauf für Job {jobid}: FAIL")
    log(f"Testlauf für Job {jobid}: PASS")
