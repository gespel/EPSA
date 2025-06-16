#include <stdio.h>

#include <eps_shm.h>
#include <eps_cpuinfo.h>

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

int populate_cpuinfo(hwloc_topology_t topology, eps_cpuinfo_t* info) {
    int socket_cnt = get_sockets_count(topology);
    int core_cnt = get_cores_count(topology);

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
