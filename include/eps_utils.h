#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <slurm/slurm.h>
#include <slurm/spank.h>
#include <slurm/slurm_errno.h>

#include <time.h>

#define TIME_STRING_SIZE 21

char* to_string(const time_t* ts);

extern void slurm_error(const char* format, ...);
void slurm_warn(const char* msg);

#endif
