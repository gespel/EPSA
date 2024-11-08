#! /bin/bash

cp ./prep_eps.so /perfacct/slurm-prep-plugins/prep_eps.so

sudo systemctl restart slurmctld.service
