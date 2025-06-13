#ifndef _EPS_CPU_H
#define _EPS_CPU_H

#include <src/interfaces/prep.h>

int process_cpus(
  job_info_msg_t* job_info_list,
  job_env_t* job_env,
  char* hostname
);

#endif
