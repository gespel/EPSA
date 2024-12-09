#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <unistd.h>

void remove_log_file(const char* log_file);
void log_message(const char* message, int fd);
void log_cgroup(pid_t pid, const char* log_file);

char* get_efp_log_file_path(int jid);
char* get_init_log_file_path(int jid);
char* get_exit_log_file_path(int jid);

int get_log_file_fd(const char* filename);

#endif
