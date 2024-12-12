#include <eps_data.h>
#include <eps_utils.h>
#include <stdlib.h>


eps_meta_data_t* get_metadata(const job_record_t* job)
{
    eps_meta_data_t* metadata = malloc(sizeof(eps_meta_data_t));

    metadata->jobid = job->job_id;
    metadata->userid = job->user_id;
    metadata->nnodes = job->node_cnt;
    metadata->tstart = job->start_time;

    return metadata;
}

void free_metadata(eps_meta_data_t* data)
{
    free(data);
}

void print_metadata(eps_meta_data_t* data)
{
    printf("\n--- META ---\n");
    printf("Job ID: %d\n", data->jobid);
    printf("User ID: %d\n", data->userid);
    printf("Nodes count: %d\n", data->nnodes);
    printf("Timestamp: %ld\n", data->tstart);
}
