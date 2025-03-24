#include <limits.h>

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#include <src/common/bitstring.h>
#include <src/interfaces/prep.h>

#define P_NAME "PrEp-EPS: "

const char plugin_name[] = "EPS PrEp plugin";
const char plugin_type[] = "prep/eps";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

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
    slurm_info("Prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    uint16_t show_flags = 0;

    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;

    job_info_msg_t* job_info_list = NULL;

    int err = slurm_load_job(&job_info_list, job_env->jobid, show_flags);
    if (err != SLURM_SUCCESS) {
        slurm_info("error: slurm_load_job: %d", err);
        return SLURM_ERROR; /* Does it make sense to continue ? */
    } 
    if (job_info_list->record_count > 0) {
        for (int i = 0; i < job_info_list->record_count; i++) {
            job_info_t job_rec = job_info_list->job_array[i];
            job_resources_t* job_resrcs = job_rec.job_resrcs;

            slurm_info("record[%d]: job_id %u", i, job_rec.job_id);
            slurm_info("record[%d]: GRES %s", i, job_rec.gres_total);

            int64_t last;
            hostlist_t* hl;
            hostlist_t* hl_last;
        
            if (job_resrcs && job_resrcs->core_bitmap &&
                ((last = bit_fls(job_resrcs->core_bitmap)) != -1)) {
                    hl = slurm_hostlist_create(job_resrcs->nodes);
                    if (!hl) {
                        slurm_info(
                            "error: hostlist_create: %s",
                            job_resrcs->nodes
                        );
                        return SLURM_ERROR; /* Does it make sense to break here instead ?*/
                    }
                    hl_last = slurm_hostlist_create(NULL);
                    if (!hl_last) {
                        slurm_info("error: hostlist_create: NULL");
                        slurm_hostlist_destroy(hl);
                        return SLURM_ERROR;
                    }

                    char hostname[HOST_NAME_MAX];
                    hostname[0] = '\0';

                    err = gethostname(hostname, HOST_NAME_MAX); 
                    if (err) {
                        perror("gethostname: ");
                        slurm_info("error: gethostname");
                        return SLURM_ERROR; /* Or break ? */
                    }

                    slurm_info("Hostname: %s", hostname);

                    int host_pos = slurm_hostlist_find(hl, hostname);
                    if (host_pos == -1) {
                        continue;
                    }

                    slurm_info("host_pos: %d", host_pos);

                    int bit_inx, bit_reps, i, sock_inx, sock_reps;
                    bit_inx = i = sock_inx = sock_reps = 0;
                    int rel_node_inx;
                    int abs_node_inx = job_rec.node_inx[i];
                    char tmp1[128], tmp2[128];

                    // gres_last = "";
                    
                    tmp1[0] = '\0'; /* tmp1[] stores the current cpu(s) allocated */
                    tmp2[0] = '\0'; /* stores last cpu(s) allocated */
                    for (
                        rel_node_inx=0;
                        rel_node_inx < job_resrcs->nhosts;
                        rel_node_inx++
                    ) {
                        slurm_info("rel_node_inx: %d", rel_node_inx);

                    //         if (sock_reps >=
                    //             job_resrcs->sock_core_rep_count[sock_inx]) {
                    //                 sock_inx++;
                    //                 sock_reps = 0;
                    //         }
                    //         sock_reps++;

                    //         bit_reps = job_resrcs->sockets_per_node[sock_inx] *
                    //                    job_resrcs->cores_per_socket[sock_inx];
                    //         host = hostlist_shift(hl);
                    //         threads = _threads_per_core(host);
                    //         cpu_bitmap = bit_alloc(bit_reps * threads);
                    //         for (j = 0; j < bit_reps; j++) {
                    //                 if (bit_test(job_resrcs->core_bitmap, bit_inx)){
                    //                         for (k = 0; k < threads; k++)
                    //                                 bit_set(cpu_bitmap,
                    //                                         (j * threads) + k);
                    //                 }
                    //                 bit_inx++;
                    //         }
                    //         bit_fmt(tmp1, sizeof(tmp1), cpu_bitmap);
                    //         FREE_NULL_BITMAP(cpu_bitmap);
                    //         /*
                    //          * If the allocation values for this host are not the
                    //          * same as the last host, print the report of the last
                    //          * group of hosts that had identical allocation values.
                    //          */
                    //         if (xstrcmp(tmp1, tmp2) ||
                    //             ((rel_node_inx < job_ptr->gres_detail_cnt) &&
                    //              xstrcmp(job_ptr->gres_detail_str[rel_node_inx],
                    //                      gres_last)) ||
                    //             (last_mem_alloc_ptr !=
                    //              job_resrcs->memory_allocated) ||
                    //             (job_resrcs->memory_allocated &&
                    //              (last_mem_alloc !=
                    //               job_resrcs->memory_allocated[rel_node_inx]))) {
                    //                 if (hostlist_count(hl_last)) {
                    //                         last_hosts =
                    //                                 hostlist_ranged_string_xmalloc(
                    //                                 hl_last);
                    //                         xstrfmtcat(out,
                    //                                    "  Nodes=%s CPU_IDs=%s "
                    //                                    "Mem=%"PRIu64" GRES=%s",
                    //                                    last_hosts, tmp2,
                    //                                    last_mem_alloc_ptr ?
                    //                                    last_mem_alloc : 0,
                    //                                    gres_last);
                    //                         xfree(last_hosts);
                    //                         xstrcat(out, line_end);

                    //                         hostlist_destroy(hl_last);
                    //                         hl_last = hostlist_create(NULL);
                    //                 }

                    //                 strcpy(tmp2, tmp1);
                    //                 if (rel_node_inx < job_ptr->gres_detail_cnt) {
                    //                         gres_last = job_ptr->
                    //                                     gres_detail_str[rel_node_inx];
                    //                 } else {
                    //                         gres_last = "";
                    //                 }
                    //                 last_mem_alloc_ptr =
                    //                         job_resrcs->memory_allocated;
                    //                 if (last_mem_alloc_ptr)
                    //                         last_mem_alloc = job_resrcs->
                    //                                 memory_allocated[rel_node_inx];
                    //                 else
                    //                         last_mem_alloc = NO_VAL64;
                    //         }
                    //         hostlist_push_host(hl_last, host);
                    //         free(host);

                    //         if (bit_inx > last)
                    //                 break;

                    //         if (abs_node_inx > job_ptr->node_inx[i+1]) {
                    //                 i += 2;
                    //                 abs_node_inx = job_ptr->node_inx[i];
                    //         } else {
                    //                 abs_node_inx++;
                    //         }
                    }

                    // if (hostlist_count(hl_last)) {
                    //         last_hosts = hostlist_ranged_string_xmalloc(hl_last);
                    //         xstrfmtcat(out, "  Nodes=%s CPU_IDs=%s Mem=%"PRIu64" GRES=%s",
                    //                  last_hosts, tmp2,
                    //                  last_mem_alloc_ptr ? last_mem_alloc : 0,
                    //                  gres_last);
                    //         xfree(last_hosts);
                    //         xstrcat(out, line_end);
                    // }
                    slurm_hostlist_destroy(hl);
                    slurm_hostlist_destroy(hl_last);
            }

        }
    }

    return SLURM_SUCCESS;
}

extern int prep_p_epilog(job_env_t* job_env, slurm_cred_t *cred)
{
    slurm_info("Epilog: %s", plugin_name);
    slurm_info("Job Id: %u", job_env->jobid);

    return SLURM_SUCCESS;
}

extern int prep_p_prolog_slurmctld(job_record_t* job_ptr, bool* async)
{
    slurm_info("Ctld_prolog: %s", plugin_name);
    slurm_info("Job Id: %u", job_ptr->job_id);

    return SLURM_SUCCESS;
}

extern int prep_p_epilog_slurmctld(job_record_t* job_ptr, bool* async)
{
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
                *required = false;
            break;
        default:
            return;
    }
    return;
}
