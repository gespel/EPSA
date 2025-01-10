#ifndef _EPS_CPU_H
#define _EPS_CPU_H

typedef struct {
    unsigned socket_cnt;
    unsigned* cores_per_socket;
    unsigned* socket_idx;
} eps_cpuinfo_t;

#endif
