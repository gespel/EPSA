#ifndef _EPS_EFP_H
#define _EPS_EFP_H

#include <stdint.h>

void
efp_main(
    uint32_t jid,
    int nodeid,
    unsigned int gres_uuid_count,
    char** gres_uuid_list,
    time_t tstart,
    const char* cpu_ids,
    eps_cpuinfo_t* cpuinfo
);

#endif
