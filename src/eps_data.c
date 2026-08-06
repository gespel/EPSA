#include <stdlib.h>
#include <limits.h>

#include <eps_data.h>
#include "src/common/uid.h"

void get_allocation_data(const job_record_t* job, eps_allocation_data_t* data)
{
    char job_name[NAME_MAX];

    if (!job->name)
    {
        snprintf(job_name, NAME_MAX, "job%d", job->job_id);
    }
    else
    {
        snprintf(job_name, NAME_MAX, "%s", job->name);
    }

    data->jobid = job->job_id;
    data->jobname = strdup(job_name);
    data->userid = job->user_id;
    data->username = strdup(uid_to_string_cached(job->user_id));
    data->nnodes = job->node_cnt;
    data->ts= job->start_time;
}

void print_allocation_data(eps_allocation_data_t* data)
{
    printf("\n--- META ---\n");
    printf("Job ID: %d\n", data->jobid);
    printf("User ID: %d (%s)\n", data->userid, data->username);
    printf("Nodes count: %d\n", data->nnodes);
    printf("Timestamp: %ld\n", data->ts);
}
