#! /bin/bash

cp ./prep_eps.so /perfacct/slurm-prep-plugins/prep_eps.so
cp ./eps.so /perfacct/slurm-plugins/eps.so

sudo systemctl restart slurmctld.service
