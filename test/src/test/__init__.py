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
import textwrap
import time
from matplotlib import pyplot as plt
import datetime
import psycopg
import random

NODES = int(os.environ.get("NODES", 1))
DURATION = int(os.environ.get("DURATION_SECONDS", 10))
PARTITION = os.environ.get("PARTITION", "")
POLL_INTERVAL = int(os.environ.get("POLL_INTERVAL", 2))
TIMEOUT = int(os.environ.get("TIMEOUT", 1800))


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


def all_nodes() -> list[str]:
    out = subprocess.run(
        ["sinfo", "-h", "-o", "%N"], capture_output=True, text=True
    ).stdout
    nodes = set()
    for nodelist in out.split():
        expanded = subprocess.run(
            ["scontrol", "show", "hostnames", nodelist], capture_output=True, text=True
        ).stdout
        nodes.update(expanded.split())
    return sorted(nodes)


def min_cpus_per_node(nodes: list[str]) -> int:
    out = subprocess.run(
        ["sinfo", "-h", "-N", "-o", "%N %c"], capture_output=True, text=True
    ).stdout
    cpus = {}
    for line in out.splitlines():
        name, count = line.split()
        cpus[name] = int(count)
    return min(cpus[node] for node in nodes if node in cpus)


def submit_job(
    cores: int,
    user: str | None = None,
    nodes: int | None = None,
    nodelist: str | None = None,
) -> str:
    n = nodes if nodes is not None else NODES
    rscript = textwrap.dedent(f"""\
        #!/bin/bash
        #SBATCH -N{n} -n{n} -c{cores}
        {f"#SBATCH -p{PARTITION}" if PARTITION else ""}
        {f"#SBATCH --uid={user}" if user else ""}
        {f"#SBATCH --nodelist={nodelist}" if nodelist else ""}
        end=$((SECONDS + {DURATION}))
        for _ in $(seq {cores}); do
            ( sum=0; i=0; while [ $SECONDS -lt $end ]; do i=$((i + 1)); sum=$((sum + i * i)); done ) &
        done
        wait
        echo "Last auf {cores} Kern(en) für {DURATION}s abgeschlossen"
        """)
    log(f"Reiche Testjob ein (Nodes={n}, Kerne={cores}, Rechenlast={DURATION}s)...")
    with tempfile.NamedTemporaryFile("w", suffix=".sbatch") as script:
        script.write(rscript)
        script.flush()
        try:
            out = subprocess.run(
                ["sbatch", script.name], capture_output=True, text=True, check=True
            ).stdout
        except subprocess.CalledProcessError as e:
            fail(f"sbatch fehlgeschlagen: {e.stderr.strip()}")

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


def check_db(jobid: str) -> float:
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
    total_energy = float(sum(
        delta * util / 100
        for name, _, delta, util in measurements
        if "package" in name
    ))

    log(f"Gesamtenergie für Job {jobid}: {total_energy:.0f} µJ")
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
    return total_energy / 3.6e12 if not errors else 0.0

def cpu_test():
    results = []
    for core_count in range(1, 9):
        jobid = submit_job(core_count)
        wait_for_completion(jobid)
        time.sleep(2)  # slurmd-Epilog Zeit geben, die Messwerte fertig zu schreiben

        results.append((core_count, check_db(jobid)))

    log("\nTestlauf abgeschlossen. Ergebnisse:")
    for cores, energy_kwh in results:
        log(f"  {cores} Kern(e): {energy_kwh:.9f} kWh")
    plt.plot([cores for cores, _ in results], [energy for _, energy in results], marker="o")
    plt.title("Energieverbrauch vs. Kernanzahl")
    plt.xlabel("Kernanzahl")
    plt.ylabel("Energieverbrauch (kWh)")
    plt.grid()
    plt.savefig(f"energy_consumption{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.png")

def short_test():
    jobid = submit_job(32)
    wait_for_completion(jobid)
    time.sleep(2)  # slurmd-Epilog Zeit geben, die Messwerte fertig zu schreiben
    energy_kwh = check_db(jobid)
    log(f"Testlauf abgeschlossen. Energieverbrauch: {energy_kwh:.9f} kWh")

def all_nodes_test(cores: int | None = None):
    nodes = all_nodes()
    if not nodes:
        fail("Keine Nodes im Cluster gefunden")
    if cores is None:
        cores = min_cpus_per_node(nodes)
        log(f"Kein cores-Wert angegeben, verwende kleinste CPU-Zahl über alle Nodes: {cores}")
    log(f"Starte Last-Job auf allen {len(nodes)} Node(s): {', '.join(nodes)}")
    jobid = submit_job(cores, nodes=len(nodes), nodelist=",".join(nodes))
    wait_for_completion(jobid)
    time.sleep(2)  # slurmd-Epilog Zeit geben, die Messwerte fertig zu schreiben
    energy_kwh = check_db(jobid)
    log(f"Testlauf abgeschlossen. Energieverbrauch: {energy_kwh:.9f} kWh")


def multiuser_test(num_jobs: int = 50):
    users = ["user1", "user2", "user3"]

    for _ in range(num_jobs):
        user = random.choice(users)
        jobid = submit_job(8, user)
        wait_for_completion(jobid)
        time.sleep(2)  # slurmd-Epilog Zeit geben, die Messwerte fertig zu schreiben
        energy_kwh = check_db(jobid)
        log(f"User {user} - Testlauf abgeschlossen. Energieverbrauch: {energy_kwh:.9f} kWh")
def main() -> None:
    for cmd in ("sbatch", "squeue"):
        if not shutil.which(cmd):
            fail(f"{cmd} nicht gefunden (Slurm installiert?)")
   # short_test()
    #cpu_test()
    all_nodes_test()
    #multiuser_test()