# EMA Plugin(s) for Slurm

Uses PreEp (Prolog/Epilog) Plugin API from Slurm to measure energy on compute
nodes and write them alongside with some additional information to connected
DB. \*

\* - Currently work in progress.

## Build

### Prerequisites

1. [CMake](https://cmake.org/)
1. [Slurm](https://www.schedmd.com/slurm/) installed and configured on the
   cluster.
2. [Slurm source code](https://github.com/SchedMD/slurm) present on the
   system used for the build.

   **IMPORTANT:** The source code version should match exactly (up to minor number)
   the version of `Slurm` installed on your cluster. Otherwise plugin compatibility
   issues may arise.

3. `EMA` library installed on all cluster nodes.
3. `PostgresQL` installed on all cluster nodes (the full installation is actually
   required only on a head node, compute nodes only need `pq` library).

### Steps

1. Clone this repo.

2. Create `build` directory in the root of this project, `cd` into it:

   ```bash
   mkdir build && cd build
   ```

3. Configure and generate build files with `cmake`:

   - via `ccmake`:
      ```bash
      ccmake ..
      ```
      Then use tui to provide required pathes.

   - via `cmake` command:
      ```bash
      cmake .. \
      -DSLURM_INSTALL_DIR=/path/to/your/slurm/installation/directory \
      -DSLURM_SRC_DIR=/path/to/slurm/sources/directory \
      -DEMA_INSTALL_DIR=/path/to/your/EMA/installation/directory \
      -DPQ_INSTALL_DIR=/path/to/your/postgresql/installation/directory
      ```

    **IMPORTANT:** You should probably build and update plugins on the target system.
    The plugin uses `NVML` library for filtering GPU devices measurements, carefully
    check `USE_NVML` option from CMake, disable it for systems where no `NVML`
    installation is available.

4. Build the plugins by running `make` inside `build` directory.

After successful completion of the above steps you should have two plugin files inside
`build` directory:

- `eps.so` (SPANK plugin)
- `prep_eps.so` (PREP plugin)

## Installation and Setup

### Plugins

1. After building the plugins copy the `.so` files from `build`
   directory to corresponding locations that `slurm` scans for the plugins.

2. Restart `Slurm` daemons:
   - `slurmd` on compute nodes;
   - `slurmctld` on the head (controller) node.

   *NOTE: On this step potentially some issue may arise (see `step 3` **IMPORTANT**
   note).*

### Database

1. Run `postgresql` server on your cluster's head node. Create a new database
   (we suggest `eps` as a name).

   Set up (create) following tables:

   ```sql
   CREATE TABLE allocations (
       id SERIAL PRIMARY KEY,
       jobid INT NOT NULL,
       job_name VARCHAR(255) NOT NULL,
       nnodes INT NOT NULL,
       userid INT NOT NULL,
       ts TIMESTAMPTZ
   );

   CREATE TABLE executions (
       id SERIAL PRIMARY KEY,
       jobid INT NOT NULL,
       node_name VARCHAR(253),
       node_id INT,
       ts_start TIMESTAMPTZ,
       ts_end TIMESTAMPTZ
   );

   CREATE TABLE measurements (
       id SERIAL PRIMARY KEY,
       exec_id INTEGER REFERENCES executions (id),
       device_name TEXT NOT NULL,
       device_uid TEXT NOT NULL,
       e0 BIGINT,
       e1 BIGINT,
       t0 BIGINT,
       t1 BIGINT,
       utilization REAL default 100
   );

   ```

2. Make connection string available via environment variable on all cluster
   nodes.

   Name of the varialble: **`EPS_DB_CONN_STR`**.
   Connection string example: `postgresql://user:password@10.0.0.42/eps`

   **Important**: As the connection string most probably will contain sensitive
   info, make sure to restrict it's availability accordingly.
