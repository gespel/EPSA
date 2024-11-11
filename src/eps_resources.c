#include <slurm/slurm.h>
#include <eps_resources.h>


eps_resources_t* get_resources(const job_record_t* job)
{
    slurm_info("Gathering job resources info...");
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

void log_resources(eps_resources_t* resrcs)
{
    if (!resrcs) return;

    slurm_info("Resources:");
    slurm_info("==========");

    slurm_info("\tCPUs count: %d", resrcs->cpu_count);
    slurm_info("\tCPUs per node: %d", resrcs->cpus_per_node);

    slurm_info("\tSockets per node: %d", resrcs->sockets_per_node);
    slurm_info("\tCores per socket: %d", resrcs->cores_per_socket);
    slurm_info("\tThreads per core: %d", resrcs->threads_per_core);

    slurm_info("\tMemory allocated: %ld", resrcs->mem_allocated);
    slurm_info("\tMemory used: %ld", resrcs->mem_used);

    slurm_info("\tWhole node: %d", resrcs->whole_node);
}
