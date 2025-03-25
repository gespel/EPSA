# EMA Plugin(s) for Slurm

Uses PreEp (Prolog/Epilog) Plugin API from Slurm to measure energy on compute
nodes and write them alongside with some additional (meta) information to the
connected database.

## Table Of Contents

1. [Limitations](#limitations)
2. [Build](#build)
   - [Prerequisites](#prerequisites)
   - [Considerations](#considerations)
   - [Steps](#steps)
3. [Installation and Setup](#installation-and-setup)
   - [Plugin](#plugin)
   - [Database](#database)
4. [Database Consistency](#database-consistency)

## Limitations

TODO: Write this section

## Build

### Prerequisites

1. [CMake](https://cmake.org/)

2. [Slurm](https://www.schedmd.com/slurm/) installed and configured on the
   cluster.

3. [Slurm source code](https://github.com/SchedMD/slurm) present on the
   system used for the build.

   **IMPORTANT:** The source code version should match exactly (up to minor number)
   the version of `Slurm` installed on your cluster. Otherwise plugin compatibility
   issues may arise.

4. [EMA](https://github.com/PERFACCT/EMA) library installed on all cluster nodes.

5. [PostgresQL](https://www.postgresql.org/) installed on all cluster nodes (the
   full installation is actually required only on the database node, compute nodes
   only need `pq` library).

6. [hwloc](https://www.open-mpi.org/projects/hwloc/) library installed on all
   cluster nodes.

### Considerations



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

1. Run `postgresql` server on your cluster's head node (or on a separate database
   node).

2. Create a new database (we suggest `eps` as a name).

3. Set up (create) following tables:

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

4. Make connection string available via environment variable on all cluster
   nodes.

   Name of the variable: **`EPS_DB_CONN_STR`**.
   Connection string example: `postgresql://user:password@10.0.0.42/eps`

   **Important**: As the connection string most probably will contain sensitive
   info, make sure to restrict it's availability accordingly.

## Database Consistency
