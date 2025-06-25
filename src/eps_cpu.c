#include <src/common/bitstring.h>

#include <eps_cpu.h>
#include <eps_utils.h>


static int _process_cpus(
  job_info_msg_t* job_info_list,
  job_env_t* job_env
)
{
    char hostname[HOST_NAME_MAX];
    hostname[0] = '\0';

    int err = gethostname(hostname, HOST_NAME_MAX);
    if (err)
    {
        perror("gethostname: ");
        slurm_info("error: gethostname");
        return 1;
    }

    for (int i = 0; i < job_info_list->record_count; i++)
    {
        job_info_t job_rec = job_info_list->job_array[i];
        if (job_rec.job_id != job_env->jobid) continue;

        job_resources_t* job_resrcs = job_rec.job_resrcs;

        if (
            !job_resrcs ||
            !job_resrcs->core_bitmap ||
            bit_fls(job_resrcs->core_bitmap) == -1
        )
        {
            /* Shoud we return an error here ? */
            continue;
        }

        int bit_reps = *job_resrcs->sockets_per_node *
                       *job_resrcs->cores_per_socket;
        uint32_t threads = _threads_per_core(hostname);
        bitstr_t* cpu_bitmap = bit_alloc(bit_reps * threads);
        int bit_idx = 0;
        for (int j = 0; j < bit_reps; j++)
        {
            if (bit_test(job_resrcs->core_bitmap, bit_idx))
            {
                for (int k = 0; k < threads; k++)
                    bit_set(cpu_bitmap, (j * threads) + k);
            }
            bit_idx++;
        }
        char cpu_ids[128];
        bit_fmt(cpu_ids, sizeof(cpu_ids), cpu_bitmap);
        slurm_info("cpu_ids: %s", cpu_ids);
        FREE_NULL_BITMAP(cpu_bitmap);
    }
    return 0;
}

int init_cpuinfo(job_env_t* job_env)
{
    uint16_t show_flags = 0;
    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;

    job_info_msg_t* job_info_list = NULL;
    int err = slurm_load_job(&job_info_list, job_env->jobid, show_flags);
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_job: %d", err);
        return 1;
    } 

    if (job_info_list->record_count > 0)
    {
      err = _process_cpus(job_info_list, job_env);
      if (err) slurm_info("error: process_cpus");
    }
    return 0;
}
