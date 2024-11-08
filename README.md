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

Repository holds two utility shell scripts:
   - `update_head.sh`
   - `update_node.sh`

for controller and compute nodes respectively.

Simply run them on corresponding nodes, in summary they just copy freshly compiled `.so`
files to required locations and restart the services.

### Suggested handy aliases

```bash
alias remake="make clean && make && make test"

alias screstart="sudo systemctl restart slurmctld.service"
alias scstatus="sudo systemctl status slurmctld.service"
alias sclog="sudo journalctl -xefu slurmctld.service"

alias sdrestart="sudo systemctl restart slurmd.service"
alias sdstatus="sudo systemctl status slurmd.service"
alias sdlog="sudo journalctl -xefu slurmd.service"
```
