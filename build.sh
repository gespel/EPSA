mkdir -p build && cd build
cmake .. \
  -DSLURM_INSTALL_DIR=/usr \
  -DSLURM_SRC_DIR=/home/sten/Uni/slurm \
  -DEMA_INSTALL_DIR=/home/sten/Uni/EMA/install \
  -DPQ_INSTALL_DIR=/usr \
  -DHWLOC_INSTALL_DIR=/usr
make prep_eps
sudo cp prep_eps.so /usr/lib/slurm/prep_eps.so
sudo chmod 755 /usr/lib/slurm/prep_eps.so
sudo systemctl restart slurmctld slurmd
