#include <limits.h>

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <src/common/bitstring.h>
#include <src/common/xstring.h>
#include <src/interfaces/prep.h>

const char plugin_name[] = "EPS";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

static int
_load_nodes(node_info_msg_t** node_buffer_pptr, uint16_t show_flags)
{
    int err;
    node_info_msg_t* node_info_ptr = NULL;
    show_flags |= SHOW_MIXED;
    err = slurm_load_node ((time_t) NULL, &node_info_ptr, show_flags);
    if (err == SLURM_SUCCESS)
    {
        *node_buffer_pptr = node_info_ptr;
    }
    return err;
}

static node_info_msg_t* _get_node_info_for_jobs(void)
{
	int err;
	node_info_msg_t *node_info_msg = NULL;
	uint16_t show_flags = 0;
	/* Must load all nodes including hidden for cross-index
	 * from job's node_inx to node table to work */

	/* Always set this flag */
	show_flags |= SHOW_ALL;

	err = _load_nodes(&node_info_msg, show_flags);
	if (err) {
            slurm_info("error: load_nodes: %d", err);
            return NULL;
	}
	return node_info_msg;
}

/* This set of functions loads/free node information so that we can map a job's
 * core bitmap to it's CPU IDs based upon the thread count on each node. */
static uint32_t _threads_per_core(char* host)
{
    node_info_msg_t *node_info_msg = NULL;
    uint32_t i, threads = 1;

    if (!host) return threads;
    if (!(node_info_msg = _get_node_info_for_jobs())) return threads;
    for (i = 0; i < node_info_msg->record_count; i++) {
        if (
            node_info_msg->node_array[i].name &&
            !xstrcmp(host, node_info_msg->node_array[i].name)
        ) {
                threads = node_info_msg->node_array[i].threads;
                break;
        }
    }
    return threads;
}

/********************************
 *
 * Slurm Plugin API hooks
 *
 ********************************/

extern int init(void)
{
    slurm_info("Init: %s", plugin_name);
    return SLURM_SUCCESS;
}

extern void fini(void)
{
    slurm_info("Fini: %s", plugin_name);
}

extern void prep_p_register_callbacks(prep_callbacks_t* callbacks) {}

extern int prep_p_prolog(job_env_t* job_env, slurm_cred_t *cred)
{
    if (!running_in_slurmd()) return SLURM_SUCCESS;

    slurm_info("Prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    char hostname[HOST_NAME_MAX];
    hostname[0] = '\0';

    int err = gethostname(hostname, HOST_NAME_MAX); 
    if (err)
    {
        perror("gethostname: ");
        slurm_info("error: gethostname");
        return SLURM_ERROR;
    }

    uint16_t show_flags = 0;

    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;
    job_info_msg_t* job_info_list = NULL;

    err = slurm_load_job(&job_info_list, job_env->jobid, show_flags);
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_job: %d", err);
        return SLURM_ERROR;
    } 
    if (job_info_list->record_count > 0)
    {
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
    }

    return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
    if (!running_in_slurmd()) return SLURM_SUCCESS;

    slurm_info("Epilog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
    if (!running_in_slurmctld()) return SLURM_SUCCESS;

    slurm_info("Ctld_prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_ptr->job_id);

    return SLURM_SUCCESS;
}

extern int prep_p_epilog_slurmctld(job_record_t* job_ptr, bool* async)
{
    if (!running_in_slurmctld()) return SLURM_SUCCESS;

    slurm_info("Ctld_epilog: %s", plugin_name);
    slurm_info("Job Id: %u", job_ptr->job_id);


    return SLURM_SUCCESS;
}

extern void prep_p_required(prep_call_type_t type, bool* required)
{
    *required = false;
    switch (type)
    {
        case PREP_PROLOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = true;
            break;
        case PREP_EPILOG_SLURMCTLD:
            if (running_in_slurmctld())
                *required = false;
            break;
        case PREP_PROLOG:
        case PREP_EPILOG:
            if (running_in_slurmd())
                *required = true;
            break;
        default:
            return;
    }
    return;
}
