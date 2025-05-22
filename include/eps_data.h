#ifndef _EPS_DATA_H
#define _EPS_DATA_H

#include "src/slurmd/slurmd/slurmd.h"
#include "src/slurmctld/slurmctld.h"


typedef struct eps_allocation_data
{
    int jobid;
    char* jobname;
    int userid;
    int nnodes;
    // TODO: Adjust type here...
    time_t ts;
} eps_allocation_data_t;

typedef struct eps_execution_data
{
    int jobid;
    const char* nodename;
    int nodeid;
    // TODO: Adjust types here...
    time_t tstart;
    time_t tend;
} eps_execution_data_t;

typedef struct eps_measurement_data
{
    int execution_id;
    const char* device_name;
    const char* device_uid;
    const char* device_type;
    unsigned long long e0;
    unsigned long long e1;
    unsigned long long t0;
    unsigned long long t1;
    double utilization;
} eps_measurement_data_t;

eps_allocation_data_t* get_allocation_data(const job_record_t* job);
void free_allocation_data(eps_allocation_data_t* data);
void print_allocation_data(eps_allocation_data_t* data);

#endif
