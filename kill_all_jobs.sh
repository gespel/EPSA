#!/usr/bin/env bash
# Killt ALLE Slurm-Jobs im Cluster, unabhängig vom Owner (Testcluster, kein
# Multi-User-Produktivbetrieb). Jobs, die trotz scancel in COMPLETING
# hängen bleiben (z.B. nach einem slurmd-Crash), werden per Node-Bounce
# (DOWN -> RESUME) freigeräumt. Zusätzlich: Nodes, die durch einen
# Prolog/Epilog-Fehler automatisch DRAINED/DOWN wurden -- auch wenn der Job
# längst nicht mehr in squeue auftaucht -- werden zurückgesetzt, und ein
# abgestürzter slurmd wird per SSH neugestartet.
set -euo pipefail

JOB_IDS="$(squeue -h -o '%A')"
if [[ -n "$JOB_IDS" ]]; then
    # shellcheck disable=SC2086
    sudo scancel $JOB_IDS
fi

ALL_NODES="$(sinfo -h -o '%N' | paste -sd,)"

echo "Warte auf Job-Cleanup..."
for _ in $(seq 1 30); do
    squeue -h | grep -q . || break
    sleep 2
done

if squeue -h | grep -q .; then
    echo "Job(s) hängen noch, räume alle Nodes per Bounce (DOWN -> RESUME) frei..."
    for node in $(scontrol show hostnames "$ALL_NODES"); do
        sudo scontrol update NodeName="$node" State=DOWN Reason="clear_stuck_job"
        sudo scontrol update NodeName="$node" State=RESUME
    done
    sleep 5
fi

echo "Prüfe slurmd auf allen Compute-Nodes..."
THIS_HOST="$(hostname -s)"
for node in $(scontrol show hostnames "$ALL_NODES"); do
    [[ "$node" == "$THIS_HOST" ]] && continue
    if ! ssh -o BatchMode=yes -o ConnectTimeout=5 "$node" "systemctl is-active --quiet slurmd" 2>/dev/null; then
        echo "  $node: slurmd nicht aktiv, starte neu..."
        ssh -o BatchMode=yes "$node" "sudo systemctl reset-failed slurmd; sudo systemctl restart slurmd" \
            || echo "  $node: Neustart fehlgeschlagen, manuell prüfen."
    fi
done

echo "Räume verbliebene DRAIN/DOWN-Nodes frei (unabhängig vom squeue-Status)..."
STUCK_NODES="$(sinfo -h -N -o '%N %t' | awk '$2 ~ /drain|down/ {print $1}')"
if [[ -n "$STUCK_NODES" ]]; then
    for node in $STUCK_NODES; do
        echo "  $node ist $(sinfo -h -N -n "$node" -o '%t'), setze auf RESUME..."
        sudo scontrol update NodeName="$node" State=RESUME
    done
fi

squeue
sinfo -N
