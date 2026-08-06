#!/usr/bin/env bash
# Runs once after the container is created.
# Sets up munge, postgres, slurm.conf and builds the plugin.
set -euo pipefail

HOSTNAME_NODE=$(hostname -s)

# ── Munge ────────────────────────────────────────────────────────────────────
if [ ! -f /etc/munge/munge.key ]; then
    dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key 2>/dev/null
    chown munge:munge /etc/munge/munge.key
    chmod 400 /etc/munge/munge.key
fi
mkdir -p /run/munge && chown munge:munge /run/munge
su -s /bin/bash munge -c "munged --foreground &"
sleep 1

# ── PostgreSQL ───────────────────────────────────────────────────────────────
PG_VERSION=$(ls /etc/postgresql)
PG_CONF_DIR="/etc/postgresql/${PG_VERSION}/main"

# Listen on all interfaces so the port can be forwarded/reached from the host
sed -i "s/^#\?listen_addresses.*/listen_addresses = '*'/" "${PG_CONF_DIR}/postgresql.conf"

# Allow password auth (fixed local devcontainer creds) from any host
if ! grep -q "^host all all all scram-sha-256$" "${PG_CONF_DIR}/pg_hba.conf" 2>/dev/null; then
    printf 'host    all             all             0.0.0.0/0               scram-sha-256\nhost    all             all             ::/0                    scram-sha-256\n' \
        >> "${PG_CONF_DIR}/pg_hba.conf"
fi

service postgresql start
sleep 2

# Create DB/user only if not already present
if ! su -s /bin/bash postgres -c "psql -tAc \"SELECT 1 FROM pg_roles WHERE rolname='eps'\"" | grep -q 1; then
    su -s /bin/bash postgres -c "psql -f /docker-entrypoint-initdb.d/init-postgres.sql"
fi

# Ensure fixed, known credentials for the local devcontainer (idempotent)
su -s /bin/bash postgres -c "psql -c \"ALTER USER eps WITH PASSWORD 'eps';\""

service postgresql restart
sleep 1

# ── slurm.conf ───────────────────────────────────────────────────────────────
NCPUS=$(nproc)
cat > /etc/slurm/slurm.conf <<EOF
ClusterName=devcluster
SlurmctldHost=${HOSTNAME_NODE}
AuthType=auth/munge
CryptoType=crypto/munge
MpiDefault=none
ProctrackType=proctrack/linuxproc
ReturnToService=1
SlurmctldPidFile=/run/slurmctld.pid
SlurmdPidFile=/run/slurmd.pid
SlurmdSpoolDir=/var/spool/slurmd
StateSaveLocation=/var/spool/slurmctld
SlurmUser=slurm
SlurmctldLogFile=/var/log/slurm/slurmctld.log
SlurmdLogFile=/var/log/slurm/slurmd.log
SlurmctldDebug=debug
SlurmdDebug=debug
# Plugin config
PrEpPlugins=prep/eps
PrologFlags=Alloc,Serial
Epilog=/bin/true
# Single-node partition
NodeName=${HOSTNAME_NODE} CPUs=${NCPUS} State=UNKNOWN
PartitionName=debug Nodes=${HOSTNAME_NODE} Default=YES MaxTime=INFINITE State=UP
EOF

mkdir -p /etc/slurm && chown slurm:slurm /etc/slurm/slurm.conf

# cgroup.conf: use v1 (v2 plugin not built)
cat > /etc/slurm/cgroup.conf <<'CGEOF'
CgroupPlugin=cgroup/v1
CGEOF

# ── Build plugin ──────────────────────────────────────────────────────────────
cd /workspace
# Remove stale host CMakeCache to avoid path conflicts inside the container
rm -f build/CMakeCache.txt build/cmake_install.cmake
mkdir -p build && cd build
cmake .. \
    -DSLURM_INSTALL_DIR=/usr \
    -DSLURM_SRC_DIR=/opt/slurm-src \
    -DEMA_INSTALL_DIR=/opt/EMA \
    -DPQ_INSTALL_DIR=/opt/pq \
    -DHWLOC_INSTALL_DIR=/opt/hwloc \
    -DUSE_NVML=OFF \
    -DUSE_MQTT=OFF
make -j"$(nproc)" prep_epsa

echo ""
echo "✓ Plugin built at /workspace/build/prep_epsa.so"
echo "  Run '.devcontainer/scripts/start-slurm.sh' to install and start Slurm."
