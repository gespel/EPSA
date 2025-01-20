#ifndef _EPS_CGROUP_H
#define _EPS_CGROUP_H

#include <unistd.h>

char* get_proc_cgroup(pid_t pid);
char* get_cpuset_restriction(pid_t pid);
int* parse_cpuset_restriction(const char* restriction, size_t* size);

int move_pid_to_cg(const char* cg_proc_file_path, pid_t pid);

#endif
