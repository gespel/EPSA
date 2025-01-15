#ifndef _EPS_CPU_H
#define _EPS_CPU_H

#include <hwloc.h>

typedef struct {
    unsigned socket_cnt;
    unsigned* cores_per_socket;
    unsigned* socket_idx;
} eps_cpuinfo_t;

int get_sockets_count(hwloc_topology_t topology);
int get_cores_count(hwloc_topology_t topology);

#endif
