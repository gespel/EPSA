#!/usr/bin/env bash
#
# Löst Slurm Power-Save-Resume für bsnode3/bsnode4 aus, indem ein trivialer
# Testjob auf beide Nodes geschickt wird, und wartet danach, bis beide Nodes
# fertig hochgefahren sind (Power-Save-State-Suffix '#'/'~' verschwunden).
# Solange sind sie noch am Booten -- danach laut SuspendTime eine Weile an
# und per SSH erreichbar, z.B. um dort install.sh laufen zu lassen.
#
# Usage:
#   ./wake_bsnodes.sh                     # Nodes: bsnode3,bsnode4
#   ./wake_bsnodes.sh nodeA,nodeB          # andere Nodeliste
#   ./wake_bsnodes.sh --loop               # alle 30s einen neuen Job starten
#   ./wake_bsnodes.sh --loop nodeA,nodeB   # kombiniert
#
# Overridable via Umgebung:
#   POLL_INTERVAL   (default: 5s)
#   TIMEOUT         (default: 600s)
#   LOOP_INTERVAL   (default: 30s, nur mit --loop)

set -euo pipefail

LOOP=0
NODES="bsnode3,bsnode4"
for arg in "$@"; do
    case "$arg" in
        --loop) LOOP=1 ;;
        --*) echo "Unbekannte Option: $arg" >&2; exit 1 ;;
        *) NODES="$arg" ;;
    esac
done

POLL_INTERVAL="${POLL_INTERVAL:-5}"
TIMEOUT="${TIMEOUT:-600}"
LOOP_INTERVAL="${LOOP_INTERVAL:-30}"

log() { echo "[$(date '+%H:%M:%S')] $*"; }

HOSTLIST="$(scontrol show hostnames "$NODES")"
NUM_NODES="$(wc -l <<<"$HOSTLIST")"

run_once() {
    log "Sende Trivial-Job an $NODES, um Power-Save-Resume auszulösen..."
    local jobid
    jobid="$(sbatch --parsable --nodelist="$NODES" --nodes="$NUM_NODES" \
        --ntasks-per-node=1 --job-name=wake_bsnodes --output=/dev/null --wrap="hostname")"
    log "Job $jobid eingereicht (siehe 'squeue -j $jobid')."

    local start=$SECONDS
    declare -A booted=()

    while true; do
        local all_up=1
        local status_line=""
        while read -r node; do
            local state
            state="$(sinfo -h -N -n "$node" -o "%t" | head -1)"
            status_line+="$node=$state  "
            if [[ "$state" == *'#'* || "$state" == *'~'* ]]; then
                all_up=0
            elif [[ -z "${booted[$node]:-}" ]]; then
                booted[$node]=$((SECONDS - start))
                log "$node hochgefahren nach ${booted[$node]}s (State: $state)"
            fi
        done <<<"$HOSTLIST"
        log "Status: $status_line"

        [[ $all_up -eq 1 ]] && break
        if (( SECONDS - start > TIMEOUT )); then
            log "TIMEOUT nach ${TIMEOUT}s -- nicht alle Nodes sind hochgefahren, breche ab."
            return 1
        fi
        sleep "$POLL_INTERVAL"
    done

    log "Alle Nodes sind hochgefahren:"
    while read -r node; do
        log "  $node: ${booted[$node]}s"
    done <<<"$HOSTLIST"
}

if [[ $LOOP -eq 1 ]]; then
    trap 'log "Beende Loop."; exit 0' INT TERM
    while true; do
        run_once || true
        log "Warte ${LOOP_INTERVAL}s bis zum nächsten Job..."
        sleep "$LOOP_INTERVAL"
    done
else
    run_once
    log "Nodes sollten laut SuspendTime jetzt eine Weile an bleiben -- per SSH erreichbar."
fi
