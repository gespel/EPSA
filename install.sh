#!/usr/bin/env bash
#
# Builds prep_epsa.so against the Slurm version actually installed on this
# node and installs it into Slurm's PluginDir, replacing any old EPS PrEp
# plugin (prep_eps.so). Must be run locally on EVERY Slurm node (controller
# and compute) -- there is no shared filesystem between them on this cluster.
#
# Usage:
#   ./install.sh                # build, install, patch slurm.conf, restart daemons
#   ./install.sh --no-restart   # skip restarting slurmctld/slurmd
#   ./install.sh --dry-run      # print what would happen, change nothing
#
# Overridable via environment:
#   EMA_INSTALL_DIR   (default: /opt/perfacct/ema)
#   SLURM_SRC_DIR     (default: auto-detected / downloaded to /usr/local/src/perfacct)
#   DEPS_DIR          (default: /usr/local/src/perfacct/deps -- pq/hwloc shim dirs)
#   SLURM_CONF        (default: /etc/slurm/slurm.conf)
#   USE_NVML          (default: OFF)
#   USE_MQTT          (default: OFF)

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRY_RUN=0
DO_RESTART=1

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        --no-restart) DO_RESTART=0 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

log()  { echo "==> $*"; }
warn() { echo "!!  $*" >&2; }
run()  { if [[ $DRY_RUN -eq 1 ]]; then echo "[dry-run] $*"; else eval "$@"; fi; }

if [[ $EUID -ne 0 ]] && ! sudo -n true 2>/dev/null; then
    warn "This script needs passwordless sudo (for install/systemctl/apt)."
    exit 1
fi

command -v slurmd >/dev/null || { warn "slurmd not found -- is Slurm installed on this node?"; exit 1; }

SLURM_VERSION="$(slurmd -V | awk '{print $2}')"          # e.g. 23.11.4
SLURM_TAG_DEFAULT="slurm-$(echo "$SLURM_VERSION" | tr . -)-1"  # e.g. slurm-23-11-4-1
log "Detected Slurm $SLURM_VERSION on $(hostname)"

EMA_INSTALL_DIR="${EMA_INSTALL_DIR:-/opt/perfacct/ema}"
DEPS_DIR="${DEPS_DIR:-/usr/local/src/perfacct/deps}"
SLURM_SRC_BASE="/usr/local/src/perfacct"
SLURM_CONF="${SLURM_CONF:-/etc/slurm/slurm.conf}"
USE_NVML="${USE_NVML:-OFF}"
USE_MQTT="${USE_MQTT:-OFF}"

# --- 1. Slurm source tree (needed for internal PrEp plugin headers) --------
if [[ -z "${SLURM_SRC_DIR:-}" ]]; then
    CANDIDATE="$SLURM_SRC_BASE/slurm-wlm-$SLURM_VERSION"
    if [[ -f "$CANDIDATE/src/slurmd/slurmd/slurmd.h" ]]; then
        SLURM_SRC_DIR="$CANDIDATE"
        log "Reusing existing Slurm source tree at $SLURM_SRC_DIR"
    else
        SLURM_TAG="${SLURM_TAG:-$SLURM_TAG_DEFAULT}"
        SLURM_SRC_DIR="$SLURM_SRC_BASE/slurm-wlm-$SLURM_VERSION"
        log "No matching Slurm source tree found, downloading tag $SLURM_TAG to $SLURM_SRC_DIR"
        run "sudo mkdir -p '$SLURM_SRC_DIR'"
        run "curl -sL 'https://github.com/SchedMD/slurm/archive/refs/tags/${SLURM_TAG}.tar.gz' | sudo tar -xzf - -C '$SLURM_SRC_DIR' --strip-components=1"
        log "Configuring Slurm source tree (generates config.h / slurm/slurm_version.h)"
        run "(cd '$SLURM_SRC_DIR' && sudo ./configure >/dev/null)"
    fi
fi
[[ -f "$SLURM_SRC_DIR/src/slurmd/slurmd/slurmd.h" ]] || { warn "SLURM_SRC_DIR=$SLURM_SRC_DIR doesn't look like a Slurm source tree"; exit 1; }
[[ -f "$SLURM_SRC_DIR/config.h" && -f "$SLURM_SRC_DIR/slurm/slurm_version.h" ]] || { warn "SLURM_SRC_DIR=$SLURM_SRC_DIR is not configured (missing config.h / slurm/slurm_version.h) -- run './configure' in it."; exit 1; }

# --- 2. EMA ------------------------------------------------------------------
if [[ -f "$EMA_INSTALL_DIR/lib/libEMA.so" && -f "$EMA_INSTALL_DIR/include/EMA.h" ]]; then
    log "EMA already installed at $EMA_INSTALL_DIR"
else
    log "EMA not found at $EMA_INSTALL_DIR, building from source (PERFACCT/EMA)"
    EMA_SRC="/usr/local/src/perfacct/EMA-src"
    run "sudo rm -rf '$EMA_SRC'"
    run "git clone --depth 1 https://github.com/PERFACCT/EMA.git '$EMA_SRC'"
    run "sudo cmake -S '$EMA_SRC' -B '$EMA_SRC/build' -DCMAKE_INSTALL_PREFIX='$EMA_INSTALL_DIR'"
    run "sudo cmake --build '$EMA_SRC/build' -- -j\"\$(nproc)\""
    run "sudo cmake --install '$EMA_SRC/build'"
    run "echo '$EMA_INSTALL_DIR/lib' | sudo tee /etc/ld.so.conf.d/perfacct-ema.conf >/dev/null"
    run "sudo ldconfig"
fi

# --- 3. pq / hwloc shim dirs (CMakeLists expects <dir>/include + <dir>/lib) --
PQ_MULTIARCH_LIB="$(find /usr/lib -maxdepth 2 -name 'libpq.so' 2>/dev/null | head -1)"
HWLOC_MULTIARCH_LIB="$(find /usr/lib -maxdepth 2 -name 'libhwloc.so' 2>/dev/null | head -1)"
if [[ -z "$PQ_MULTIARCH_LIB" || -z "$HWLOC_MULTIARCH_LIB" ]]; then
    log "libpq-dev / libhwloc-dev missing, installing"
    run "sudo apt-get update -qq"
    run "sudo apt-get install -y -qq libpq-dev hwloc libhwloc-dev"
    PQ_MULTIARCH_LIB="$(find /usr/lib -maxdepth 2 -name 'libpq.so' 2>/dev/null | head -1)"
    HWLOC_MULTIARCH_LIB="$(find /usr/lib -maxdepth 2 -name 'libhwloc.so' 2>/dev/null | head -1)"
fi

PQ_INSTALL_DIR="$DEPS_DIR/pq"
HWLOC_INSTALL_DIR="$DEPS_DIR/hwloc"
run "sudo mkdir -p '$PQ_INSTALL_DIR/include' '$PQ_INSTALL_DIR/lib' '$HWLOC_INSTALL_DIR/include' '$HWLOC_INSTALL_DIR/lib'"
run "sudo cp -rn /usr/include/postgresql/. '$PQ_INSTALL_DIR/include/'"
run "sudo ln -sf '$PQ_MULTIARCH_LIB' '$PQ_INSTALL_DIR/lib/libpq.so'"
run "sudo ln -sf /usr/include/hwloc.h '$HWLOC_INSTALL_DIR/include/hwloc.h'"
run "sudo ln -sf /usr/include/hwloc '$HWLOC_INSTALL_DIR/include/hwloc'"
run "sudo ln -sf '$HWLOC_MULTIARCH_LIB' '$HWLOC_INSTALL_DIR/lib/libhwloc.so'"

# --- 4. Build ------------------------------------------------------------
log "Configuring build (SLURM_SRC_DIR=$SLURM_SRC_DIR, EMA_INSTALL_DIR=$EMA_INSTALL_DIR)"
run "cmake -S '$REPO_DIR' -B '$REPO_DIR/build' \
    -DSLURM_INSTALL_DIR=/usr \
    -DSLURM_SRC_DIR='$SLURM_SRC_DIR' \
    -DEMA_INSTALL_DIR='$EMA_INSTALL_DIR' \
    -DPQ_INSTALL_DIR='$PQ_INSTALL_DIR' \
    -DHWLOC_INSTALL_DIR='$HWLOC_INSTALL_DIR' \
    -DUSE_NVML=$USE_NVML -DUSE_MQTT=$USE_MQTT"
log "Building prep_epsa.so"
run "cmake --build '$REPO_DIR/build' --target prep_epsa -- -j\"\$(nproc)\""

# --- 5. Locate Slurm's PluginDir -------------------------------------------
SCONTROL_CONFIG="$(scontrol show config 2>/dev/null || true)"
PLUGIN_DIR="$(awk -F'= *' '/^PluginDir/{print $2; exit}' <<<"$SCONTROL_CONFIG")"
if [[ -z "$PLUGIN_DIR" ]]; then
    DPKG_LIST="$(dpkg -L slurm-wlm-basic-plugins 2>/dev/null || true)"
    PLUGIN_DIR="$(dirname "$(grep -m1 prep_script.so <<<"$DPKG_LIST" || true)")"
fi
[[ -n "$PLUGIN_DIR" && -d "$PLUGIN_DIR" ]] || { warn "Could not determine Slurm PluginDir"; exit 1; }
log "Slurm PluginDir: $PLUGIN_DIR"

# --- 6. Remove any old EPS PrEp plugin --------------------------------------
BACKUP_DIR="/usr/local/src/perfacct/OLD-EPS-backup"
TS="$(date +%Y%m%d%H%M%S)"
for old in "$PLUGIN_DIR"/prep_eps.so "$PLUGIN_DIR"/prep_epsa.so; do
    if [[ -e "$old" ]]; then
        run "sudo mkdir -p '$BACKUP_DIR'"
        log "Backing up existing $(basename "$old") to $BACKUP_DIR/"
        run "sudo mv '$old' '$BACKUP_DIR/$(basename "$old").$TS'"
    fi
done

# --- 7. Install new plugin ---------------------------------------------------
log "Installing prep_epsa.so into $PLUGIN_DIR"
run "sudo install -m 755 '$REPO_DIR/build/prep_epsa.so' '$PLUGIN_DIR/prep_epsa.so'"

# --- 8. Patch slurm.conf ------------------------------------------------------
if [[ -f "$SLURM_CONF" ]]; then
    run "sudo cp '$SLURM_CONF' '${SLURM_CONF}.bak.$TS'"
    if grep -q '^PrepPlugins=prep/eps$' "$SLURM_CONF"; then
        log "Switching PrepPlugins=prep/eps -> prep/epsa in $SLURM_CONF"
        run "sudo sed -i 's|^PrepPlugins=prep/eps\$|PrepPlugins=prep/epsa|' '$SLURM_CONF'"
    elif ! grep -q '^PrepPlugins=prep/epsa$' "$SLURM_CONF"; then
        warn "No 'PrepPlugins=prep/eps(a)' line found in $SLURM_CONF -- add 'PrepPlugins=prep/epsa' manually."
    fi

    EPILOG_PATH="$(awk -F= '/^Epilog=/{print $2; exit}' "$SLURM_CONF")"
    if [[ -n "$EPILOG_PATH" && ! -f "$EPILOG_PATH" ]]; then
        warn "Epilog=$EPILOG_PATH in $SLURM_CONF does not exist -- prep_p_epilog would never run."
        log "Setting Epilog=/bin/true (dummy trigger is sufficient, see EPSA.md)"
        run "sudo sed -i 's|^Epilog=.*\$|Epilog=/bin/true|' '$SLURM_CONF'"
    elif [[ -z "$EPILOG_PATH" ]]; then
        warn "No Epilog= set in $SLURM_CONF -- prep_p_epilog will be skipped for jobs with no active steps at termination (see EPSA.md pitfall #1). Add 'Epilog=/bin/true'."
    fi

    if ! grep -q '^PrologFlags=.*Alloc' "$SLURM_CONF"; then
        warn "PrologFlags in $SLURM_CONF doesn't include 'Alloc' -- recommended per README for accurate measurements."
    fi
else
    warn "$SLURM_CONF not found, skipping slurm.conf patching"
fi

# --- 9. EPS_DB_CONN_STR sanity check ----------------------------------------
if ! sudo grep -rq 'EPS_DB_CONN_STR' /etc/slurm/eps-db-env /etc/default/slurmd /etc/default/slurmctld 2>/dev/null; then
    warn "EPS_DB_CONN_STR not found in /etc/slurm/eps-db-env or /etc/default/{slurmd,slurmctld}."
    warn "Set it (see README.md) or slurmd/slurmctld won't be able to write measurements."
fi

# --- 10. Restart whichever daemons run on this node -------------------------
if [[ $DO_RESTART -eq 1 ]]; then
    RESTART_UNITS=()
    if systemctl is-enabled slurmctld >/dev/null 2>&1; then RESTART_UNITS+=(slurmctld); fi
    if systemctl is-enabled slurmd >/dev/null 2>&1; then RESTART_UNITS+=(slurmd); fi
    if [[ ${#RESTART_UNITS[@]} -gt 0 ]]; then
        log "Restarting: ${RESTART_UNITS[*]}"
        run "sudo systemctl restart ${RESTART_UNITS[*]}"
        sleep 2
        run "sudo systemctl --no-pager status ${RESTART_UNITS[*]} | grep -E 'Active|Loaded' || true"
    else
        warn "Neither slurmctld nor slurmd is enabled on this node -- nothing to restart."
    fi
else
    log "Skipping daemon restart (--no-restart). Plugin won't be loaded until slurmctld/slurmd is restarted."
fi

log "Done."
