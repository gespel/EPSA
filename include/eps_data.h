#ifndef _EPS_DATA_H
#define _EPS_DATA_H

#include "src/slurmd/slurmd/slurmd.h"
#include "src/slurmctld/slurmctld.h"


typedef struct eps_allocation_data
{
    int jobid;
    int userid;
    int nnodes;
    // TODO: Adjust type here...
    time_t ts;
} eps_allocation_data_t;

eps_allocation_data_t* get_allocation_data(const job_record_t* job);
void free_allocation_data(eps_allocation_data_t* data);
void print_allocation_data(eps_allocation_data_t* data);

#endif
