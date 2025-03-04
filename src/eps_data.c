#include <stdlib.h>
#include <limits.h>

#include <eps_data.h>



eps_allocation_data_t* get_allocation_data(const job_record_t* job)
{
    eps_allocation_data_t* data = malloc(sizeof(eps_allocation_data_t));

    char job_name[NAME_MAX];

    if (!job->name) {
        snprintf(job_name, NAME_MAX, "job%d", job->job_id);
    } else {
        snprintf(job_name, NAME_MAX, "%s", job->name);
    }

    data->jobid = job->job_id;
    data->jobname = strdup(job_name);
    data->userid = job->user_id;
    data->nnodes = job->node_cnt;
    // TODO: Save it in the right format (ts + tz_info, maybe as string)...
    data->ts= job->start_time;

    return data;
}

void free_allocation_data(eps_allocation_data_t* data)
{
    free(data->jobname);
    free(data);
}

void print_allocation_data(eps_allocation_data_t* data)
{
    printf("\n--- META ---\n");
    printf("Job ID: %d\n", data->jobid);
    printf("User ID: %d\n", data->userid);
    printf("Nodes count: %d\n", data->nnodes);
    printf("Timestamp: %ld\n", data->ts);
}
