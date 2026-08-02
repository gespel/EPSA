#!/usr/bin/env bash
# Install the built plugin and (re)start Slurm daemons.
# Run this after every rebuild of prep_eps.so.
set -euo pipefail

PLUGIN_SRC=/workspace/build/prep_eps.so
PLUGIN_DST=/usr/lib/slurm/prep_eps.so

if [ ! -f "$PLUGIN_SRC" ]; then
    echo "ERROR: $PLUGIN_SRC not found. Run 'make prep_eps' first." >&2
    exit 1
fi

cp "$PLUGIN_SRC" "$PLUGIN_DST"
chmod 755 "$PLUGIN_DST"
echo "Installed $PLUGIN_DST"

# Ensure munge is running
if ! pgrep -x munged >/dev/null; then
    mkdir -p /run/munge && chown munge:munge /run/munge
    su -s /bin/bash munge -c "munged"
    sleep 1
fi

# Ensure PostgreSQL is running
service postgresql start 2>/dev/null || true

# Kill stale EFP child processes that would block slurmd restart
pgrep -a slurmd 2>/dev/null | grep -v "^$(pgrep -ox slurmd 2>/dev/null)" \
    | awk '{print $1}' | xargs -r kill -9 2>/dev/null || true

# Stop existing daemons cleanly
pkill -x slurmctld 2>/dev/null || true
pkill -x slurmd    2>/dev/null || true
sleep 1

# Ensure required directories exist with correct ownership
mkdir -p /var/spool/slurmctld /var/spool/slurmd /var/log/slurm /run/munge
chown slurm:slurm /var/spool/slurmctld /var/spool/slurmd /var/log/slurm

# Source env files (normally done by init scripts)
[ -f /etc/default/slurmctld ] && set -a && . /etc/default/slurmctld && set +a
[ -f /etc/default/slurmd ]    && set -a && . /etc/default/slurmd    && set +a

# Start daemons (built-from-source, no init scripts)
slurmctld
sleep 1
slurmd

sleep 2
echo ""
echo "✓ Slurm daemons started. Check status with:"
echo "  sinfo"
echo "  squeue"
echo "  tail -f /var/log/slurm/slurmctld.log"
echo "  tail -f /var/log/slurm/slurmd.log"
