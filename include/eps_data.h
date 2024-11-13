#ifndef _EPS_DATA_H
#define _EPS_DATA_H

#include <EMA.h>

#include "eps_resources.h"

#include "src/slurmd/slurmd/slurmd.h"
#include "src/slurmctld/slurmctld.h"


typedef struct eps_meta_data
{
    int jobid;
    int userid;
    int nnodes;
    char* tstart;
    eps_resources_t* resources;
} eps_meta_data_t;

typedef struct eps_job_data
{
    int jobid;
    unsigned long long energy;
    char* tstart;
    unsigned long long duration;
    long tstart_posix;
    unsigned long long runtime;
} eps_job_data_t;

typedef struct eps_device_data
{
    int jobid;
    char* nodename;
    unsigned long long energy;
    char* tstart;
    long tstart_posix;
    unsigned long long duration;
    unsigned long long runtime;
    char* device;
    void* device_type; /* EMA plugin type? */
    // TODO: Clarify what this should be...
    void* resource;
    int exclusive;
} eps_device_data_t;

eps_meta_data_t* get_metadata(const job_record_t* job);
void free_metadata(eps_meta_data_t* data);
void print_metadata(eps_meta_data_t* data);

eps_job_data_t* get_job_data(
    const job_record_t* job, unsigned long long energy);
void free_job_data(eps_job_data_t* data);
void print_job_data(eps_job_data_t* data);

eps_device_data_t* get_device_data(
    const Device* device,
    const job_env_t* job,
    unsigned long long energy,
    const time_t* tstart,
    long tstart_posix,
    unsigned long long tstart_us,
    unsigned long long tend_us
);
void free_device_data(eps_device_data_t* data);
void print_device_data(eps_device_data_t* data);

#endif
