FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── Build deps ───────────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    # base toolchain
    build-essential cmake git curl pkg-config \
    # Slurm build deps
    autotools-dev automake flex bison \
    libssl-dev libpam0g-dev \
    # Munge (Slurm auth)
    munge libmunge-dev \
    # PostgreSQL full install + client lib
    postgresql postgresql-client libpq-dev \
    # hwloc
    hwloc libhwloc-dev \
    # Python (test runner)
    python3 python3-pip python3-dev libpython3-dev \
    # shell
    zsh \
    # misc
    sudo vim gdb strace lsof \
    && rm -rf /var/lib/apt/lists/*

# ── Compat prefix dirs for CMakeLists -D*_INSTALL_DIR paths ─────────────────
# hwloc: lib is in multiarch dir; create /opt/hwloc/{include,lib}
RUN mkdir -p /opt/hwloc/include /opt/hwloc/lib \
 && ln -s /usr/include/hwloc.h /opt/hwloc/include/hwloc.h \
 && ln -s /usr/include/hwloc   /opt/hwloc/include/hwloc \
 && ln -s /usr/lib/x86_64-linux-gnu/libhwloc.so /opt/hwloc/lib/libhwloc.so

# libpq: headers live under /usr/include/postgresql/; create /opt/pq/{include,lib}
RUN mkdir -p /opt/pq/include /opt/pq/lib \
 && cp -r /usr/include/postgresql/. /opt/pq/include/ \
 && ln -s /usr/lib/x86_64-linux-gnu/libpq.so /opt/pq/lib/libpq.so

# ── Slurm 26.05.2 from source (must match host version) ─────────────────────
ARG SLURM_TAG=slurm-26-05-2-1
RUN useradd -r -s /sbin/nologin -d /var/lib/slurm slurm 2>/dev/null || true
RUN mkdir -p /opt/slurm-src \
    && curl -sL "https://github.com/SchedMD/slurm/archive/refs/tags/${SLURM_TAG}.tar.gz" \
       | tar -xzf - -C /opt/slurm-src --strip-components=1

RUN cd /opt/slurm-src \
    && ./configure \
        --prefix=/usr \
        --sysconfdir=/etc/slurm \
        --with-munge \
        --with-pam_dir=/usr/lib/x86_64-linux-gnu/security \
        --disable-x11 \
        --disable-man-doc \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig

# ── EMA (Energy Measurement Agent) ──────────────────────────────────────────
RUN git clone --depth 1 https://github.com/PERFACCT/EMA.git /opt/EMA-src \
    && mkdir -p /opt/EMA-src/build \
    && cd /opt/EMA-src/build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/opt/EMA \
    && make -j"$(nproc)" \
    && make install \
    && echo "/opt/EMA/lib" > /etc/ld.so.conf.d/ema.conf \
    && ldconfig

# ── Slurm directory structure ────────────────────────────────────────────────
RUN mkdir -p \
        /var/spool/slurmctld \
        /var/spool/slurmd \
        /var/log/slurm \
        /var/log/eps \
        /run/munge \
        /etc/slurm \
    && chown slurm:slurm /var/spool/slurmctld /var/spool/slurmd /var/log/slurm \
    && chmod 755 /var/log/eps

# ── Make EMA lib accessible to slurm user ───────────────────────────────────
RUN cp /opt/EMA/lib/libEMA.so /usr/local/lib/ \
    && echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf \
    && ldconfig

# ── DB connection env for both daemons ──────────────────────────────────────
RUN printf 'EPS_DB_CONN_STR=postgresql://eps:eps@localhost/eps\n' \
        >> /etc/default/slurmd \
    && printf 'EPS_DB_CONN_STR=postgresql://eps:eps@localhost/eps\n' \
        >> /etc/default/slurmctld

COPY .devcontainer/scripts/init-postgres.sql /docker-entrypoint-initdb.d/init-postgres.sql

# Python test dependencies
RUN pip3 install --no-cache-dir "psycopg[binary]"

# ── rebuild command ──────────────────────────────────────────────────────────
RUN cat > /usr/local/bin/rebuild <<'EOF'
#!/usr/bin/env zsh
set -euo pipefail

if [[ ! -f /workspace/build/CMakeCache.txt ]]; then
    echo "==> No CMake cache found, configuring from scratch..."
    mkdir -p /workspace/build
    cmake -S /workspace -B /workspace/build \
        -DSLURM_INSTALL_DIR=/usr \
        -DSLURM_SRC_DIR=/opt/slurm-src \
        -DEMA_INSTALL_DIR=/opt/EMA \
        -DPQ_INSTALL_DIR=/opt/pq \
        -DHWLOC_INSTALL_DIR=/opt/hwloc \
        -DUSE_NVML=OFF \
        -DUSE_MQTT=OFF
fi

echo "==> Building prep_epsa..."
cmake --build /workspace/build --target prep_epsa -- -j"$(nproc)"

echo "==> Installing plugin..."
install -m 755 /workspace/build/prep_epsa.so /usr/lib/slurm/prep_epsa.so
echo "==> Restarting Slurm daemons..."
pkill -x slurmctld 2>/dev/null || true
pkill -x slurmd    2>/dev/null || true
sleep 1

set -a
[[ -f /etc/default/slurmctld ]] && source /etc/default/slurmctld
[[ -f /etc/default/slurmd    ]] && source /etc/default/slurmd
set +a

slurmctld
sleep 1
slurmd

echo "==> Done. $(sinfo 2>/dev/null || true)"
EOF
RUN chmod +x /usr/local/bin/rebuild

RUN cat > /usr/local/bin/clean <<'EOF'
#!/usr/bin/env zsh
set -euo pipefail
rm -rf build
EOF
RUN chmod +x /usr/local/bin/clean

# ── Oh My Zsh ────────────────────────────────────────────────────────────────
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended
ENV SHELL=/usr/bin/zsh

# ── uv (Astral) ──────────────────────────────────────────────────────────────
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:${PATH}"

WORKDIR /workspace
