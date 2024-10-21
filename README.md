# EMA Plugin for Slurm

Uses PreEp (Prolog/Epilog) Plugin API from Slurm to measure enery on compute
nodes and write them alongside with some additional information to connected
DB.

## Build

- `cd` into project's root directory.
- run `make` to build plugin's `.so` file.
- run `make test` to build dummy test executable that you can later use for
  tests with `srun`.

## Update nodes

This repository holds two bash scripts to speed up and simplify the process
of cluster nodes updation/reloading after rebuilding the plugin:

   - `update_head.sh` - for use on cluster's head node.
   - `update_node.sh` - for use on cluster's compute nodes.

You can run them or inspect their contents to learn which steps are required to
perform them manually later.

### Suggested handy aliases

```bash
alias updh="/path/to/root/dir/update_head.sh"
alias updn="/path/to/root/dir/update_node.sh"
```
