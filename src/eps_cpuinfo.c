#include <eps_cpuinfo.h>

int get_sockets_count(hwloc_topology_t topology){
    int count = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PACKAGE);
    return count;
}

int get_cores_count(hwloc_topology_t topology) {
    int count = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    return count;
}
