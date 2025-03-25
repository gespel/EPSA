#ifndef _EPS_EFP_H
#define _EPS_EFP_H

void efp_main(
    int jid,
    int nodeid,
    time_t tstart,
    const char* cpu_ids,
    eps_cpuinfo_t* cpuinfo
);
void efp_main(int jid, unsigned int gres_uuid_count, char** gres_uuid_list);

#endif
