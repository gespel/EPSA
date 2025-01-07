# EMA Plugin(s) for Slurm

Uses PreEp (Prolog/Epilog) Plugin API from Slurm to measure energy on compute
nodes and write them alongside with some additional information to connected
DB. \*

\* - Currently work in progress.

## Build

- `cd` into project's root directory;
- run `make` to build plugins `.so` files;
- run `make test` to build dummy test executables that you can later use for
  tests with `srun` or standalone;

## Update nodes

1. After building the plugins you need to copy the `.so` files to corresponding
   locations that `slurm` scans for the plugins.

2. You need to restart `slurm` daemons:
   - `slurmd` on compute nodes;
   - `slurmctld` on the head (controller) node.

### Suggested handy aliases

```bash
alias remake="make clean && make && make test"

alias screstart="sudo systemctl restart slurmctld.service"
alias scstatus="sudo systemctl status slurmctld.service"
alias sclog="sudo journalctl -xefu slurmctld.service"k

alias sdrestart="sudo systemctl restart slurmd.service"
alias sdstatus="sudo systemctl status slurmd.service"
alias sdlog="sudo journalctl -xefu slurmd.service"
```
