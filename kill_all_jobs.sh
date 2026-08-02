#!/usr/bin/env bash
# Killt alle eigenen Slurm-Jobs. Jobs, die trotz scancel in COMPLETING
# hängen bleiben (z.B. nach einem slurmd-Crash), werden per Node-Bounce
# (DOWN -> RESUME) freigeräumt.
set -euo pipefail

scancel -u "$USER"
sleep 2

if squeue -h -u "$USER" | grep -q .; then
    echo "Job(s) hängen noch, räume alle Nodes per Bounce (DOWN -> RESUME) frei..."
    for node in $(scontrol show hostnames "$(sinfo -h -o '%N' | paste -sd,)"); do
        sudo scontrol update NodeName="$node" State=DOWN Reason="clear_stuck_job"
        sudo scontrol update NodeName="$node" State=RESUME
    done
    sleep 5
fi

squeue -u "$USER"
