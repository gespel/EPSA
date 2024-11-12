#include <eps_data.h>
#include <eps_utils.h>

#include <limits.h>

#include <slurm/slurm.h>
#include <stdlib.h>
#include <unistd.h>


eps_meta_data_t* get_metadata(const job_record_t* job)
{
    eps_meta_data_t* metadata = malloc(sizeof(eps_meta_data_t));

    metadata->jobid = job->job_id;
    metadata->userid = job->user_id;

    metadata->nnodes = job->node_cnt;

    metadata->tstart = to_string(&job->start_time);

    metadata->resources = get_resources(job);

    return metadata;
}

void free_metadata(eps_meta_data_t* data)
{
    free(data->resources);
    free(data->tstart);
    free(data);
}

void log_metadata(eps_meta_data_t* data)
{
    slurm_info("Job ID: %d", data->jobid);
    slurm_info("User ID: %d", data->userid);
    slurm_info("Nodes count: %d", data->nnodes);
    slurm_info("Timestamp: %s", data->tstart);
    log_resources(data->resources);
}

eps_job_data_t* get_job_data(
    const job_record_t* job, unsigned long long energy)
{
    eps_job_data_t* data = malloc(sizeof(eps_job_data_t));

    data->jobid = job->job_id;
    data->energy = energy;
    data->tstart = to_string(&job->start_time);
    data->duration = job->end_time - job->start_time;

    return data;
}

void free_job_data(eps_job_data_t* data)
{
    free(data->tstart);
    free(data);
}

void log_job_data(eps_job_data_t* data)
{
    slurm_info("Job ID: %d", data->jobid);
    slurm_info("Energy: %lld", data->energy);
    slurm_info("Time started: %s", data->tstart);
    slurm_info("Duration: %lld", data->duration);
}

eps_device_data_t* get_device_data(
    const Device* device,
    const job_env_t* job,
    unsigned long long energy,
    const time_t* tstart,
    unsigned long long tstart_us,
    unsigned long long tend_us
)
{
    eps_device_data_t* data = malloc(sizeof(eps_device_data_t));

    data->jobid = job->jobid;

    char nodename[HOST_NAME_MAX];
    int err = gethostname(nodename, HOST_NAME_MAX);
    data->nodename = err ? NULL : nodename;

    data->device = (char*)EMA_get_device_name(device);
    //TODO: Realize device type extraction...
    data->device_type = NULL;

    data->energy = energy;

    data->tstart = to_string(tstart);
    data->duration = tend_us - tstart_us;

    // TODO: Find a way to write something meaningfull here...
    data->resource = NULL;
    data->exclusive = 0;

    return data;
}

void free_device_data(eps_device_data_t* data)
{
    // TODO: Remove the latch here later...
    if (data->resource) free(data->resource);
    free(data->tstart);
    free(data);
}

void log_device_data(eps_device_data_t* data)
{
    slurm_info("Job ID: %d", data->jobid);
    slurm_info("Node name: %s", data->nodename);
    slurm_info("Device: %s", data->device);
    slurm_info("Energy: %lld", data->energy);
    slurm_info("Time started: %s", data->tstart);
    slurm_info("Duration: %lld", data->duration);
}
