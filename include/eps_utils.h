#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <stdio.h>
#include <stdint.h>

#include <slurm/slurm.h>

#include <src/common/xstring.h>

int file_exist(const char* path);

char* get_suffixed_name(const char* base, uint32_t jid);
char* get_efp_log_file_path(uint32_t jid);
char* get_init_log_file_path(uint32_t jid);
char* get_exit_log_file_path(uint32_t jid);

uint32_t threads_per_core(char* host);

FILE* get_log_file_fd(const char* filename);

int get_nodeid();

int parse_gres(const char* gres, char** idx);
int eps_parse_int(const char* str, int* val);
unsigned int* parse_index_range(const char* range, size_t* size);

#define LOG(FD, MSG, ...) do { \
        fprintf(FD, MSG "\n", ##__VA_ARGS__); \
    } while (0)
#endif
