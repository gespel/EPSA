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
   the version of `Slurm` installed on your cluster. Otherwise plugin compatability
   issues may arise.

3. `EMA` library installed on all cluster nodes.

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
      -DEMA_INSTALL_DIR=/path/to/your/EMA/installation/directory
      ```

4. Build the plugins by running `make` inside `build` directory.

After successfull completion of the above steps you should have two plugin files inside
`build` directory:

- `eps.so` (SPANK plugin)
- `prep_eps.so` (PREP plugin)

## Installation

1. After building the plugins copy the `.so` files from `build`
   directory to corresponding locations that `slurm` scans for the plugins.

2. You need to restart `slurm` daemons:
   - `slurmd` on compute nodes;
   - `slurmctld` on the head (controller) node.
