#ifndef _EPS_CPU_H
#define _EPS_CPU_H

#include <hwloc.h>

typedef struct {
    unsigned core_cnt;
    unsigned socket_cnt;
    unsigned* cores_per_socket;
    unsigned* socket_idx;
} eps_cpuinfo_t;

int get_sockets_count(hwloc_topology_t topology);
int get_cores_count(hwloc_topology_t topology);

int populate_cpuinfo(hwloc_topology_t topology, eps_cpuinfo_t* info);

void free_cpuinfo(eps_cpuinfo_t* info);

#endif
