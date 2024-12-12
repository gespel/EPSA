#ifndef _EPS_RESRCS_H
#define _EPS_RESRCS_H

#include <stdint.h>

#include "src/slurmctld/slurmctld.h"

typedef struct
{
    uint32_t cpu_count;
    uint16_t cpus_per_node;

    uint16_t sockets_per_node;
    uint16_t cores_per_socket;
    uint16_t threads_per_core;

    uint64_t mem_allocated;
    uint64_t mem_used;

    // INFO: this is for --exclusive handling
    uint8_t whole_node;
} eps_resources_t;

eps_resources_t* get_resources(const job_record_t* job);
void print_resources(eps_resources_t* resrcs);

#endif
