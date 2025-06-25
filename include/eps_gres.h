#ifndef _EPS_GRES_H
#define _EPS_GRES_H

#include <src/interfaces/prep.h>

int process_gres(
    char** gres_uuid_list,
    int* gres_uuid_count,
    node_info_msg_t* node_info_list
);

#endif
