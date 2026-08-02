#!/usr/bin/env bash
#
# Führt einen lokalen End-to-End-Testlauf des prep_eps-Plugins durch, wie in
# EPSA.md ("Ergebnisse in der DB prüfen") beschrieben:
#   1. Einen Batch-Testjob via sbatch absetzen.
#   2. Auf dessen Abschluss warten.
#   3. In der EPS-Datenbank prüfen, ob allocations/executions/measurements
#      für diesen Job befüllt wurden.
#
# Voraussetzung: slurmctld/slurmd laufen bereits mit installiertem
# prep_eps-Plugin (siehe EPSA.md), EPS_DB_CONN_STR ist gesetzt.

set -euo pipefail

NODES="${NODES:-1}"
DURATION_SECONDS="${DURATION_SECONDS:-30}"
PARTITION="${PARTITION:-}"
POLL_INTERVAL="${POLL_INTERVAL:-2}"
TIMEOUT="${TIMEOUT:-180}"
DB_CONN_STR="${EPS_DB_CONN_STR:-}"

log()  { printf '[test] %s\n' "$*"; }
fail() { printf '[test] FEHLER: %s\n' "$*" >&2; exit 1; }

# Auf der lokalen Entwicklermaschine läuft slurmctld bereits mit gesetzter
# EPS_DB_CONN_STR (siehe EPSA.md, "Bekannte Stolperfallen" Punkt 2). Ist die
# Variable in dieser Shell nicht gesetzt, aus dem environ des laufenden
# Daemons übernehmen, statt das DB-Passwort hier im Skript zu hinterlegen.
if [ -z "$DB_CONN_STR" ]; then
    CTLD_PID="$(pgrep -x slurmctld | head -n1 || true)"
    if [ -n "$CTLD_PID" ]; then
        DB_CONN_STR="$(sudo -n cat "/proc/${CTLD_PID}/environ" 2>/dev/null \
            | tr '\0' '\n' | sed -n 's/^EPS_DB_CONN_STR=//p')"
        [ -n "$DB_CONN_STR" ] && log "EPS_DB_CONN_STR aus laufendem slurmctld (PID ${CTLD_PID}) übernommen."
    fi
fi

command -v sbatch >/dev/null 2>&1 || fail "sbatch nicht gefunden (Slurm installiert?)"
command -v squeue >/dev/null 2>&1 || fail "squeue nicht gefunden (Slurm installiert?)"
command -v psql   >/dev/null 2>&1 || fail "psql nicht gefunden (PostgreSQL-Client installiert?)"
[ -n "$DB_CONN_STR" ] || fail "EPS_DB_CONN_STR ist nicht gesetzt und konnte nicht automatisch aus slurmctld ermittelt werden"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

SBATCH_SCRIPT="$WORKDIR/test.sbatch"
{
    echo "#!/bin/bash"
    echo "#SBATCH -N${NODES} -n${NODES}"
    [ -n "$PARTITION" ] && echo "#SBATCH -p${PARTITION}"
    # Triviale CPU-Last statt sleep, damit tatsächlich messbar Energie
    # verbraucht wird (ein reiner sleep hält die CPU idle).
    echo "end=\$((SECONDS + ${DURATION_SECONDS}))"
    echo "sum=0; i=0"
    echo "while [ \$SECONDS -lt \$end ]; do i=\$((i + 1)); sum=\$((sum + i * i)); done"
    echo "echo \"Summe der Quadrate nach \$i Iterationen: \$sum\""
} > "$SBATCH_SCRIPT"

log "Reiche Testjob ein (Nodes=${NODES}, Rechenlast=${DURATION_SECONDS}s)..."
SUBMIT_OUTPUT="$(sbatch "$SBATCH_SCRIPT")"
JOBID="$(printf '%s' "$SUBMIT_OUTPUT" | grep -oE '[0-9]+$')"
[ -n "$JOBID" ] || fail "Konnte JobID nicht aus sbatch-Ausgabe lesen: ${SUBMIT_OUTPUT}"
log "Job ${JOBID} eingereicht, warte auf Abschluss (Timeout ${TIMEOUT}s)..."

ELAPSED=0
while squeue -h -j "$JOBID" >/dev/null 2>&1 && [ -n "$(squeue -h -j "$JOBID" 2>/dev/null)" ]; do
    if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
        fail "Job ${JOBID} ist nach ${TIMEOUT}s nicht abgeschlossen (Timeout)"
    fi
    sleep "$POLL_INTERVAL"
    ELAPSED=$((ELAPSED + POLL_INTERVAL))
done
log "Job ${JOBID} abgeschlossen."

# Ein kurzer Moment, damit der slurmd-Epilog die Messwerte fertig schreiben kann.
sleep 2

log "Prüfe Datenbankeinträge für Job ${JOBID}..."

ALLOC_COUNT="$(psql "$DB_CONN_STR" -tAc "SELECT count(*) FROM allocations WHERE jobid = ${JOBID};")"
EXEC_COUNT="$(psql "$DB_CONN_STR" -tAc "SELECT count(*) FROM executions WHERE jobid = ${JOBID};")"
MEASUREMENT_COUNT="$(psql "$DB_CONN_STR" -tAc \
    "SELECT count(*) FROM measurements m JOIN executions e ON m.exec_id = e.id WHERE e.jobid = ${JOBID};")"
BAD_DELTA_COUNT="$(psql "$DB_CONN_STR" -tAc \
    "SELECT count(*) FROM measurements m JOIN executions e ON m.exec_id = e.id WHERE e.jobid = ${JOBID} AND m.e1 <= m.e0;")"

log "allocations=${ALLOC_COUNT} executions=${EXEC_COUNT} measurements=${MEASUREMENT_COUNT} measurements_mit_e1<=e0=${BAD_DELTA_COUNT}"

log "Gemessener Energieverbrauch für Job ${JOBID}:"
psql "$DB_CONN_STR" -c \
    "SELECT device_name, device_type, e1 - e0 AS energy_delta, utilization \
     FROM measurements m JOIN executions e ON m.exec_id = e.id \
     WHERE e.jobid = ${JOBID} ORDER BY m.id;"

TOTAL_ENERGY="$(psql "$DB_CONN_STR" -tAc \
    "SELECT COALESCE(SUM(m.e1 - m.e0), 0) FROM measurements m JOIN executions e ON m.exec_id = e.id WHERE e.jobid = ${JOBID};")"
log "Gesamtenergie (Summe aller Devices) für Job ${JOBID}: ${TOTAL_ENERGY} µJ"
log "Gesamtenergie in kWh: $(awk "BEGIN {printf \"%.9f\", ${TOTAL_ENERGY} / 3.6e12}")"

FAILED=0
[ "$ALLOC_COUNT" -ge 1 ]       || { log "FEHLER: kein Eintrag in allocations"; FAILED=1; }
[ "$EXEC_COUNT" -ge 1 ]        || { log "FEHLER: kein Eintrag in executions"; FAILED=1; }
[ "$MEASUREMENT_COUNT" -ge 2 ] || { log "FEHLER: weniger als 2 Einträge in measurements"; FAILED=1; }
[ "$BAD_DELTA_COUNT" -eq 0 ]   || { log "FEHLER: ${BAD_DELTA_COUNT} measurement(s) mit e1 <= e0"; FAILED=1; }

if [ "$FAILED" -ne 0 ]; then
    log "Testlauf für Job ${JOBID}: FAIL"
    exit 1
fi

log "Testlauf für Job ${JOBID}: PASS"
