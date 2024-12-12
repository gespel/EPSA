#ifndef _EPS_DATA_H
#define _EPS_DATA_H

#include "src/slurmd/slurmd/slurmd.h"
#include "src/slurmctld/slurmctld.h"


typedef struct eps_meta_data
{
    int jobid;
    int userid;
    int nnodes;
    time_t tstart;
} eps_meta_data_t;

eps_meta_data_t* get_metadata(const job_record_t* job);
void free_metadata(eps_meta_data_t* data);
void print_metadata(eps_meta_data_t* data);

#endif
