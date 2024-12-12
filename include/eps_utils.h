#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <unistd.h>

#define LOG_MSG_BUFF_SIZE 128

void remove_log_file(const char* log_file);
void log_message(const char* message, int fd);
void log_cgroup(pid_t pid, const char* log_file);

char* get_suffixed_name(const char* base, int jid);
char* get_efp_log_file_path(int jid);
char* get_init_log_file_path(int jid);
char* get_exit_log_file_path(int jid);

int get_log_file_fd(const char* filename);

#define LOG(BUF, FD, MSG, ...) do { \
        sprintf(BUF, MSG "\n", ##__VA_ARGS__); \
        log_message(BUF, FD); \
    } while (0)

#endif
