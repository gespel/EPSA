#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <unistd.h>

void remove_log_file(const char* log_file);
void log_message(const char* message, const char* log_file);
void log_cgroup(pid_t pid, const char* log_file);

#endif
