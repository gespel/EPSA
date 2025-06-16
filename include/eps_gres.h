#ifndef _EPS_GRES_H
#define _EPS_GRES_H

#include <src/interfaces/prep.h>

int process_gres_count(
    node_info_t node_rec,
    char* idx,
    unsigned int* gres_idxs,
    size_t* gres_count
);

#endif
