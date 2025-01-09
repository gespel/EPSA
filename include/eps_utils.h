#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <stdio.h>

char* get_suffixed_name(const char* base, int jid);
char* get_efp_log_file_path(int jid);
char* get_init_log_file_path(int jid);
char* get_exit_log_file_path(int jid);

FILE* get_log_file_fd(const char* filename);

#define LOG(FD, MSG, ...) do { \
        fprintf(FD, MSG "\n", ##__VA_ARGS__); \
    } while (0)

#endif
