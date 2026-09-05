# EPSA – Build, Installation & Local Testing

Practical notes on building, installing, and locally testing the `prep_eps`
plugin. Complements the [README.md](./README.md) (which covers the
prerequisites in detail) with everything needed for a concrete local
single-node test run, including a few pitfalls encountered during testing.

## Table Of Contents

1. [Building](#building)
2. [Installing](#installing)
3. [Configuring a Local Test Cluster](#configuring-a-local-test-cluster)
4. [Known Pitfalls](#known-pitfalls)
5. [Important Slurm Commands](#important-slurm-commands)
6. [Checking Results In The DB](#checking-results-in-the-db)

## Building

```bash
mkdir -p build && cd build
cmake .. \
  -DSLURM_INSTALL_DIR=/usr \
  -DSLURM_SRC_DIR=/path/to/slurm-source \
  -DEMA_INSTALL_DIR=/path/to/EMA/install \
  -DPQ_INSTALL_DIR=/usr \
  -DHWLOC_INSTALL_DIR=/usr
make prep_eps
```

Result: `build/prep_eps.so`.

- `SLURM_SRC_DIR` must match the installed Slurm version exactly (except for
  the minor version) (see `slurmd -V`).
- After changes to `src/*.c`, running `make prep_eps` in the `build`
  directory is enough for an incremental rebuild.

## Installing

```bash
sudo cp build/prep_eps.so /usr/lib/slurm/prep_eps.so
sudo chmod 755 /usr/lib/slurm/prep_eps.so
```

`/usr/lib/slurm` is the default `PluginDir` that `slurmd`/`slurmctld` search
with the Arch package `slurm-llnl` (or the path configured under
`PluginDir=` in `slurm.conf`).

After every rebuild: copy the file again and restart `slurmctld`+`slurmd`
(plugins are only loaded when the daemon starts):

```bash
sudo systemctl restart slurmctld slurmd
```

## Configuring a Local Test Cluster

Minimal setup for a controller + compute node on the same host:

- **Munge**: `munge.key` present, `munge.service` running.
- **PostgreSQL**: database `eps` with the tables `allocations`,
  `executions`, `measurements` (see `src/eps_db.c` /
  `include/eps_data.h` for columns).
- **`slurm.conf`** (key points, rest as in `slurm.conf.example`):
  ```
  PrEpPlugins=prep/eps
  PrologFlags=Alloc,Serial
  Epilog=/bin/true
  ```
- **`EPS_DB_CONN_STR`** must be set for **both** daemons (see
  [Known Pitfalls](#known-pitfalls)):
  - `/etc/default/slurmd`
  - `/etc/default/slurmctld`
  ```
  EPS_DB_CONN_STR=postgresql://eps:<password>@localhost/eps
  ```

## Known Pitfalls

These points cost the most time during the first local test run:

1. **`Epilog=` must be set, otherwise the PrEp epilog never runs.**
   `slurmd` has a bypass in `_rpc_terminate_job()`
   (`src/slurmd/slurmd/req.c`): if there are no more active steps when the
   terminate RPC arrives (`nsteps == 0` – true for every normally finished
   job, regardless of runtime) **and** no `Epilog=` script is configured,
   `run_epilog()` – and thus `prep_p_epilog` – is skipped entirely. Without a
   (dummy) `Epilog=` script (e.g. `/bin/true`), `executions`/`measurements`
   always stay empty, no matter how the job behaves.

2. **`EPS_DB_CONN_STR` is needed by both `slurmd` and `slurmctld`.**
   `prep_p_prolog_slurmctld`/`prep_p_epilog_slurmctld` run in the
   `slurmctld` process, `prep_p_prolog`/`prep_p_epilog` in the `slurmd`
   process – each opens its own DB connection independently.

3. **`libEMA.so` must be reachable for the `slurm` system user.** If the
   EMA installation lives under a home directory with restrictive
   permissions (e.g. `700`), `slurmctld` (running as user `slurm`) cannot
   `dlopen` the plugin (`libEMA.so: cannot open shared object file`), even
   though `ldd` looks fine as the dev user. Fix: make the lib available
   system-wide as well, e.g.:
   ```bash
   sudo cp /path/to/EMA/install/lib/libEMA.so /usr/local/lib/
   echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/eps-ema.conf
   sudo ldconfig
   ```

4. **Orphaned EFP child processes block `slurmd` restarts.** The EFP
   measurement process is forked in the prolog and stays alive until
   signaled by the epilog. If the epilog doesn't run for whatever reason
   (see point 1, or a crash midway), the EFP process hangs forever as
   `EFP waiting...` (log: `/var/log/eps/efp_<jobid>.log`) – while holding
   open the listening socket inherited from `slurmd`, which blocks a
   restart with `Address already in use`. Identify the affected PIDs
   (`ps aux | grep "slurmd --systemd"`, all except the current main
   process) and `kill -9` them before restarting `slurmd`.

5. **Node without configured GRES**: In Slurm versions with the fixed
   `parse_gres`/`_process_gres_count` (see `src/eps_utils.c`,
   `src/eps_gres.c`), this is no longer an issue – before the fix, a node
   with no `Gres=` at all in `slurm.conf` led to a `slurmd` crash
   (`strdup(NULL)`), or afterwards to a failed prolog including `DRAIN` of
   the node.

6. **Jobs get stuck with `launch failed requeued held`** (log:
   `cannot setup the scope for cgroup`): The systemd scope
   `slurmstepd.scope` is dead (`journalctl -u slurmstepd.scope` shows
   `Deactivated`), usually because its keep-alive process was terminated
   along with a previous `slurmd` restart. Fix: `sudo systemctl restart
   slurmctld slurmd` – this makes systemd recreate the scope.

## Important Slurm Commands

### Services

```bash
sudo systemctl restart munge postgresql slurmctld slurmd
sudo systemctl status slurmctld slurmd --no-pager
journalctl -u slurmctld -u slurmd -f          # follow live
sudo tail -f /var/log/slurm-llnl/slurmd.log
sudo tail -f /var/log/slurm-llnl/slurmctld.log
```

### Cluster/Node Status

```bash
sinfo                          # partition and node overview
scontrol show node <name>      # detailed status of a node
scontrol update NodeName=<name> State=RESUME   # bring a node out of DRAIN/DOWN
```

### Starting Test Jobs

```bash
# Interactive, blocking, good for quick checks
srun -N1 -n1 sleep 5

# Batch job (closer to real production behavior)
cat > test.sbatch <<'EOF'
#!/bin/bash
#SBATCH -N1 -n1
sleep 30
EOF
sbatch test.sbatch
```

### Watching Jobs

```bash
squeue                         # queue / running jobs
scontrol show job <jobid>      # detailed status of a job
scancel <jobid>                # cancel a job (e.g. if stuck in the prolog)
```

### Increasing Debug Level (if needed)

```bash
sudo scontrol setdebug debug3
sudo scontrol setdebugflags +Agent +Protocol
```
For persistently more detail in `slurmd`'s log: set `SlurmdDebug=debug` in
`slurm.conf` and restart `slurmd` (otherwise the above `debug()`-level lines
in `req.c` won't show up in the log at all).

## Checking Results In The DB

```bash
psql "postgresql://eps:<password>@localhost/eps" \
  -c "SELECT * FROM allocations ORDER BY jobid;" \
  -c "SELECT * FROM executions ORDER BY id;" \
  -c "SELECT id, exec_id, device_name, device_type, e1-e0 AS energy_delta, utilization FROM measurements ORDER BY id;"
```

A successful test run shows:
- One entry in `allocations` (from the `slurmctld` prolog, as soon as the
  job is allocated).
- One entry in `executions` per node/job (from the `slurmd` epilog).
- Two or more entries in `measurements` per `execution` (e.g.
  `CPU-0.package-0` and `CPU-0.core`), with `e1 > e0` as the measured
  energy delta.
