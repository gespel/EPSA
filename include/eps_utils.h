#ifndef _EPS_UTILS_H
#define _EPS_UTILS_H

#include <stdio.h>
#include <stdint.h>

#include <slurm/slurm.h>

#include <src/common/xstring.h>
int file_exist(const char* path);

#ifdef HAS_NVML
#define NVML_HANDLE_RET(RET, FN) do { \
    if (RET != NVML_SUCCESS) \
    { \
        slurm_info("error: " FN ": %s", nvmlErrorString(RET)); \
        free(gres_uuid_list); \
        free(gres_idxs); \
        return SLURM_ERROR; \
    } \
} while(0)
#endif

char* get_suffixed_name(const char* base, uint32_t jid);
char* get_efp_log_file_path(uint32_t jid);
char* get_init_log_file_path(uint32_t jid);
char* get_exit_log_file_path(uint32_t jid);

int _load_nodes(node_info_msg_t** node_buffer_pptr, uint16_t show_flags);
node_info_msg_t* _get_node_info_for_jobs(void);
uint32_t _threads_per_core(char* host);

FILE* get_log_file_fd(const char* filename);

int eps_parse_int(const char* str, int* val);
int is_range(const char* value, int* o_start, int* o_end);
int* parse_range(const char* range, size_t* size);
int parse_gres(const char* gres, char** idx);
int eps_parse_int(const char* str, int* val);
int* parse_cpuset_restriction(const char* restriction, size_t* size);

#define LOG(FD, MSG, ...) do { \
        fprintf(FD, MSG "\n", ##__VA_ARGS__); \
    } while (0)

#endif
