#include <stdio.h>

#include <src/common/bitstring.h>
#include <src/common/hostlist.h>
#include <src/common/job_resources.h>

#include <eps_shm.h>
#include <eps_cpuinfo.h>
#include <eps_utils.h>


static int _get_sockets_count(hwloc_topology_t topology)
{
    int count = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PACKAGE);
    return count;
}

static int _get_cores_count(hwloc_topology_t topology)
{
    int count = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    return count;
}

int populate_cpuinfo(hwloc_topology_t topology, eps_cpuinfo_t* info)
{
    int socket_cnt = _get_sockets_count(topology);
    if (socket_cnt == 0)
    {
        printf("error: hwloc returned socket objects count 0");
        return 1;
    }

    int core_cnt = _get_cores_count(topology);
    if (socket_cnt == 0)
    {
        printf("error: hwloc returned core objects count 0");
        return 1;
    }

    if (socket_cnt == -1)
        printf("warn: multiple levels with socket objects detected");
    if (core_cnt == -1)
        printf("warn: multiple levels with core objects detected");

    if (socket_cnt == -1 || core_cnt == -1)
    {
        // TODO: Handle multi level topologies ?
        printf("error: multi level topology detected");
        return 1;
    }

    int* cores_per_socket = calloc(socket_cnt, sizeof(int));
    int* socket_idx = calloc(core_cnt, sizeof(int));

    for (int core_idx = 0; core_idx < core_cnt; ++core_idx) {
        hwloc_obj_t core_obj = hwloc_get_obj_by_type(
            topology,
            HWLOC_OBJ_CORE,
            core_idx
        );
        if (!core_obj) {
            printf(
                "error: "
                "hwloc_get_obj_by_type: failed to get core with idx: %d",
                core_idx
            );
            free(cores_per_socket);
            free(socket_idx);
            return 1;
        }

        hwloc_obj_t socket_obj = core_obj;
        int parent_socket_found = socket_obj->type == HWLOC_OBJ_SOCKET;

        while (!parent_socket_found && socket_obj->parent) {
            socket_obj = socket_obj->parent;
            parent_socket_found = socket_obj->type == HWLOC_OBJ_SOCKET;
        }

        if (!parent_socket_found) {
            printf(
                "Failed to get parent socket for core with idx: %d",
                core_idx
            );
            // CONSIDER: Break and return error here?
            continue;
        }

        cores_per_socket[socket_obj->logical_index]++;
        socket_idx[core_idx] = socket_obj->logical_index;
    }

    info->socket_cnt = socket_cnt;
    info->core_cnt = core_cnt;
    info->cores_per_socket = cores_per_socket;
    info->socket_idx = socket_idx;

    return 0;
}

static int _process_cpus(
  job_info_msg_t* job_info_list,
  job_env_t* job_env,
  char* cpu_ids
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
        if (job_rec.job_id != EPS_JOB_ID(job_env)) continue;

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

        // core_bitmap spans ALL nodes of the allocation concatenated
        // (Node_0's sockets/cores, then Node_1's, ...) -- slice out just
        // this node's range, otherwise cpu_ids ends up with core indexes
        // that belong to a different node and don't exist locally.
        hostlist_t* hl = hostlist_create(job_resrcs->nodes);
        if (!hl)
        {
            slurm_info(
                "error: hostlist_create failed for '%s'", job_resrcs->nodes
            );
            return 1;
        }
        int node_id = hostlist_find(hl, hostname);
        hostlist_destroy(hl);
        if (node_id < 0)
        {
            slurm_info(
                "error: node '%s' not found in job node list '%s'",
                hostname, job_resrcs->nodes
            );
            return 1;
        }

        int start = get_job_resources_offset(
            job_resrcs, (uint32_t)node_id, 0, 0
        );
        if (start < 0)
        {
            slurm_info(
                "error: get_job_resources_offset failed for node_id %d",
                node_id
            );
            return 1;
        }
        int end = bit_size(job_resrcs->core_bitmap);
        if ((uint32_t)(node_id + 1) < job_resrcs->nhosts)
        {
            int next = get_job_resources_offset(
                job_resrcs, (uint32_t)(node_id + 1), 0, 0
            );
            if (next >= 0) end = next;
        }

        int local_len = end - start;
        if (local_len <= 0)
        {
            slurm_info(
                "error: invalid local core range [%d,%d) for node_id %d",
                start, end, node_id
            );
            return 1;
        }

        bitstr_t* local_bitmap = bit_alloc(local_len);
        for (int b = 0; b < local_len; b++)
        {
            if (bit_test(job_resrcs->core_bitmap, start + b))
                bit_set(local_bitmap, b);
        }
        bit_fmt(cpu_ids, CPU_IDS_SIZE, local_bitmap);
        bit_free(local_bitmap);
        slurm_info("cpu_ids: %s", cpu_ids);
    }
    return 0;
}

int init_cpuinfo(job_env_t* job_env, char* cpu_ids)
{
    uint16_t show_flags = 0;
    show_flags |= SHOW_ALL;
    show_flags |= SHOW_DETAIL;

    job_info_msg_t* job_info_list = NULL;
#ifdef HAVE_JOB_ENV_JOBID
    int err = slurm_load_job(&job_info_list, EPS_JOB_ID(job_env), show_flags);
#else
    int err = slurm_load_job(&job_info_list, job_env->step_id, show_flags);
#endif
    if (err != SLURM_SUCCESS)
    {
        slurm_info("error: slurm_load_job: %d", err);
        return 1;
    } 

    if (job_info_list->record_count > 0)
    {
        err = _process_cpus(job_info_list, job_env, cpu_ids);
        slurm_free_job_info_msg(job_info_list);
        if (err) slurm_info("error: process_cpus");
    }
    return 0;
}
