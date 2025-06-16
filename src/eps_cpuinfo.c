#include <stdio.h>

#include <src/common/bitstring.h>

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

int process_cpus(
  job_info_msg_t* job_info_list,
  job_env_t* job_env,
  char* hostname,
  char* cpu_ids
)
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
        uint32_t threads = threads_per_core(hostname);
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
        bit_fmt(cpu_ids, sizeof(cpu_ids), cpu_bitmap);
        slurm_info("cpu_ids: %s", cpu_ids);
        FREE_NULL_BITMAP(cpu_bitmap);
    }
  return 0;
}
