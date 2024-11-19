#include <eps_resources.h>
#include <eps_utils.h>


eps_resources_t* get_resources(const job_record_t* job)
{
    INFO("Gathering job resources info...");
    eps_resources_t* resrcs = malloc(sizeof(eps_resources_t));

    resrcs->cpu_count = job->job_resrcs->ncpus;
    resrcs->cpus_per_node = *job->job_resrcs->cpus;

    resrcs->sockets_per_node = *job->job_resrcs->sockets_per_node;
    resrcs->cores_per_socket = *job->job_resrcs->cores_per_socket;
    resrcs->threads_per_core = job->job_resrcs->threads_per_core;

    resrcs->mem_allocated = *job->job_resrcs->memory_allocated;
    resrcs->mem_used = *job->job_resrcs->memory_used;

    resrcs->whole_node = job->job_resrcs->whole_node;

    return resrcs;
}

void print_resources(eps_resources_t* resrcs)
{
    if (!resrcs) return;

    printf("Resources:\n");
    printf("==========\n");

    printf("\tCPUs count: %d\n", resrcs->cpu_count);
    printf("\tCPUs per node: %d\n", resrcs->cpus_per_node);

    printf("\tSockets per node: %d\n", resrcs->sockets_per_node);
    printf("\tCores per socket: %d\n", resrcs->cores_per_socket);
    printf("\tThreads per core: %d\n", resrcs->threads_per_core);

    printf("\tMemory allocated: %ld\n", resrcs->mem_allocated);
    printf("\tMemory used: %ld\n", resrcs->mem_used);

    printf("\tWhole node: %d\n", resrcs->whole_node);
    printf("==========\n");
}
