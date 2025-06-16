#ifndef _EPS_NVML_H
#define _EPS_NVML_H

int nvml_process_gres(
  unsigned int* gres_idxs,
  char** gres_uuid_list,
  int* gres_uuid_count,
  size_t gres_count
);

#endif
