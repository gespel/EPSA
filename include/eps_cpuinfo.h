#ifndef _EPS_CPUINFO_H
#define _EPS_CPUINFO_H

#include <hwloc.h>

#include <src/interfaces/prep.h>

typedef struct {
    int core_cnt;
    int socket_cnt;
    int* cores_per_socket;
    int* socket_idx;
} eps_cpuinfo_t;

int populate_cpuinfo(hwloc_topology_t topology, eps_cpuinfo_t* info);

#define CPU_IDS_SIZE 128

int init_cpuinfo(
  job_env_t* job_env,
  char* cpu_ids
);

#endif
