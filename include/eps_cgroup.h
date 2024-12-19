#ifndef _EPS_CGROUP_H
#define _EPS_CGROUP_H

#include <unistd.h>

char* get_proc_cgroup(pid_t pid);

int move_pid_to_cg(const char* cg_proc_file_path, pid_t pid);

#endif
