#ifndef _EPS_DATA_H
#define _EPS_DATA_H

#include "src/slurmd/slurmd/slurmd.h"
#include "src/slurmctld/slurmctld.h"


typedef struct eps_allocation_data
{
    int jobid;
    char* jobname;
    int userid;
    char* username;
    int nnodes;
    time_t ts;
} eps_allocation_data_t;

typedef struct eps_execution_data
{
    int jobid;
    const char* nodename;
    int nodeid;
    int userid;
    time_t tstart;
    time_t tend;
} eps_execution_data_t;

typedef struct eps_measurement_data
{
    int execution_id;
    int userid;
    const char* device_name;
    const char* device_uid;
    const char* device_type;
    unsigned long long e0;
    unsigned long long e1;
    unsigned long long t0;
    unsigned long long t1;
    double utilization;
} eps_measurement_data_t;

void get_allocation_data(const job_record_t* job, eps_allocation_data_t*);
void print_allocation_data(eps_allocation_data_t* data);

#endif
