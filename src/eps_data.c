#include <eps_data.h>
#include <eps_utils.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>


eps_meta_data_t* get_metadata(const job_record_t* job)
{
    eps_meta_data_t* metadata = malloc(sizeof(eps_meta_data_t));

    metadata->jobid = job->job_id;
    metadata->userid = job->user_id;
    metadata->nnodes = job->node_cnt;
    metadata->tstart = job->start_time;
    metadata->resources = get_resources(job);

    return metadata;
}

void free_metadata(eps_meta_data_t* data)
{
    free(data->resources);
    free(data);
}

void print_metadata(eps_meta_data_t* data)
{
    printf("\n--- META ---\n");
    printf("Job ID: %d\n", data->jobid);
    printf("User ID: %d\n", data->userid);
    printf("Nodes count: %d\n", data->nnodes);
    printf("Timestamp: %ld\n", data->tstart);
    print_resources(data->resources);
}

eps_device_data_t* get_device_data(
    const Device* device,
    const job_env_t* job,
    unsigned long long energy,
    time_t tstart,
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
    data->energy = energy;
    data->tstart = tstart;
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
    free(data);
}

void print_device_data(eps_device_data_t* data)
{
    printf("Job ID: %d\n", data->jobid);
    printf("Node name: %s\n", data->nodename);
    printf("Device: %s\n", data->device);
    printf("Energy: %lld\n", data->energy);
    printf("Time started: %ld\n", data->tstart);
    printf("Duration: %lld\n", data->duration);
}
