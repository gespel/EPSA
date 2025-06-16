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

int process_cpus(
  job_info_msg_t* job_info_list,
  job_env_t* job_env,
  char* hostname,
  char* cpu_ids
);

#endif
