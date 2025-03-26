# EMA Plugin(s) for Slurm

Uses PreEp (Prolog/Epilog) Plugin API from Slurm to measure energy on compute
nodes and write them alongside with some additional (meta) information to the
connected database.

The database server can be set up on the dedicated node or the head (controller)
node of the cluster.

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

Currently the underlying measurements tool (EMA) is not capable of measuring separate
CPU cores. That makes it impossible to distinguish with 100% accuracy the energy
consumptions of two jobs that share the same socket on the same node.

We introduce the `utilization` column in our `measurements` SQL table (in EPS
database) that aim to provide an estimation for every job, based on the node's
topology (obtained via `hwloc`) and allocated cores information (derived from
`Slurm`). For example: if 2 cores from 4 available on the socket were allocated
the utilization value will be 50%.

We suggest/recommend to setup the cluster with EPS plugin in the way that all
allocations are exclusive (reserving the whole node/socket). This will allow
for higher energy consumption measurements accuracy.

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

The plugin uses `NVML` internally for those nodes that have NVIDIA GPUs
configured as generic resources. The build system has a configurable
option `USE_NVML`. You can use this option to disable the `NVML` on build time
for systems without GPUs, or without `NVML` library installed, but keep in mind
that for latter case the plugin then would not be able to measure GPU devices
if they are present and configured on the node.

We recommend to execute the build steps from the following section on the target
systems. However it is possible to build plugins once and distribute
across nodes if you have homogeneous cluster/partition setup.

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
      Then use tui to provide required pathes and toggle options.

   - via `cmake` command:
      ```bash
      cmake .. \
      -DSLURM_INSTALL_DIR=/path/to/your/slurm/installation/directory \
      -DSLURM_SRC_DIR=/path/to/slurm/sources/directory \
      -DEMA_INSTALL_DIR=/path/to/your/EMA/installation/directory \
      -DPQ_INSTALL_DIR=/path/to/your/postgresql/installation/directory \
      -DHWLOC_INSTALL_DIR=/path/to/your/hwloc/installation/directory
      ```

    **IMPORTANT:** Check [considerations](#considerations) section if you
    haven't done that.

4. Build the plugins by running `make` inside `build` directory.

After successful completion of the above steps you should a plugin file inside
`build` directory called `eps.so`.

## Installation and Setup

### Plugins

1. After building the plugins copy the `.so` files from `build`
   directory to corresponding locations that `slurm` scans for the plugins.

2. Restart `Slurm` daemons:
   - `slurmd` on compute nodes;
   - `slurmctld` on the head (controller) node.

   *NOTE: On this step potentially some issue may arise (see
   [Considerations](#considerations)).*

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

As can be seen in previous section `allocations` table does not have any
explicit (foreign key) relations with `executions` table, however it is
assumed that column `nnodes` value from `allocations` should always equal to
the count of `executions` rows (with unique `node_name` and `node_id` column
values) for the same job (`jobid`). If it is not so it can be a sign of some
failures or unexpected behaviour from either `Slurm` or `EPS` plugin that
require inspection and addressing.

To identify such inconsistencies in the database use the following query:

```sql
SELECT a.jobid, a.nnodes AS nodes_allocated, COUNT(e.id) AS executions
FROM allocations a LEFT JOIN executions e ON a.jobid = e.jobid
GROUP BY a.jobid, a.nnodes
HAVING a.nnodes <> COUNT(e.id);
```

*NOTE: It is planned for the future to provide convenient tooling allowing to 
simplify and/or automate those consistency checks.*
