#ifndef _EPS_CPU_H
#define _EPS_CPU_H

#include <hwloc.h>

typedef struct {
    int core_cnt;
    int socket_cnt;
    int* cores_per_socket;
    int* socket_idx;
} eps_cpuinfo_t;

int get_sockets_count(hwloc_topology_t topology);
int get_cores_count(hwloc_topology_t topology);

int populate_cpuinfo(hwloc_topology_t topology, eps_cpuinfo_t* info);
int map_cpuinfo(eps_cpuinfo_t* info, int fd);

void free_cpuinfo(eps_cpuinfo_t* info);

#endif
