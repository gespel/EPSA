#!/usr/bin/env bash
#
# Löst Slurm Power-Save-Resume für bsnode3/bsnode4 aus, indem ein trivialer
# Testjob auf beide Nodes geschickt wird, und wartet danach, bis beide Nodes
# fertig hochgefahren sind (Power-Save-State-Suffix '#'/'~' verschwunden).
# Solange sind sie noch am Booten -- danach laut SuspendTime eine Weile an
# und per SSH erreichbar, z.B. um dort install.sh laufen zu lassen.
#
# Usage:
#   ./wake_bsnodes.sh                # Nodes: bsnode3,bsnode4
#   ./wake_bsnodes.sh nodeA,nodeB    # andere Nodeliste
#
# Overridable via Umgebung:
#   POLL_INTERVAL   (default: 5s)
#   TIMEOUT         (default: 600s)

set -euo pipefail

NODES="${1:-bsnode3,bsnode4}"
POLL_INTERVAL="${POLL_INTERVAL:-5}"
TIMEOUT="${TIMEOUT:-600}"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

HOSTLIST="$(scontrol show hostnames "$NODES")"
NUM_NODES="$(wc -l <<<"$HOSTLIST")"

log "Sende Trivial-Job an $NODES, um Power-Save-Resume auszulösen..."
JOBID="$(sbatch --parsable --nodelist="$NODES" --nodes="$NUM_NODES" \
    --ntasks-per-node=1 --job-name=wake_bsnodes --output=/dev/null --wrap="hostname")"
log "Job $JOBID eingereicht (siehe 'squeue -j $JOBID')."

START=$SECONDS
declare -A booted=()

while true; do
    ALL_UP=1
    STATUS_LINE=""
    while read -r node; do
        STATE="$(sinfo -h -N -n "$node" -o "%t" | head -1)"
        STATUS_LINE+="$node=$STATE  "
        if [[ "$STATE" == *'#'* || "$STATE" == *'~'* ]]; then
            ALL_UP=0
        elif [[ -z "${booted[$node]:-}" ]]; then
            booted[$node]=$((SECONDS - START))
            log "$node hochgefahren nach ${booted[$node]}s (State: $STATE)"
        fi
    done <<<"$HOSTLIST"
    log "Status: $STATUS_LINE"

    [[ $ALL_UP -eq 1 ]] && break
    if (( SECONDS - START > TIMEOUT )); then
        log "TIMEOUT nach ${TIMEOUT}s -- nicht alle Nodes sind hochgefahren, breche ab."
        exit 1
    fi
    sleep "$POLL_INTERVAL"
done

log "Alle Nodes sind hochgefahren:"
while read -r node; do
    log "  $node: ${booted[$node]}s"
done <<<"$HOSTLIST"

log "Nodes sollten laut SuspendTime jetzt eine Weile an bleiben -- per SSH erreichbar."
