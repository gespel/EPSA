#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <eps_utils.h>

#define LOG_MODE 0640

#define SUFFIX_MAX_LENGTH 15

#define LOG_DIR_PATH "/var/log/eps"

#define EFP_LOG_PATH_BASE LOG_DIR_PATH "/efp_"
#define TINIT_LOG_PATH_BASE LOG_DIR_PATH "/task_init_"
#define TEXIT_LOG_PATH_BASE LOG_DIR_PATH "/task_exit_"


char* get_suffixed_name(const char* base, uint32_t jid) {
    size_t size = strlen(base) + SUFFIX_MAX_LENGTH;
    char* name = calloc(size, sizeof(char));
    snprintf(name, size, "%s%d.log", base, jid);
    return name;
}

char* get_efp_log_file_path(uint32_t jid) {
    return get_suffixed_name(EFP_LOG_PATH_BASE, jid);
}

char* get_init_log_file_path(uint32_t jid) {
    return get_suffixed_name(TINIT_LOG_PATH_BASE, jid);
}

char* get_exit_log_file_path(uint32_t jid) {
    return get_suffixed_name(TEXIT_LOG_PATH_BASE, jid);
}

FILE* get_log_file_fd(const char* filename) {
    struct stat st = {0};
    if (stat(LOG_DIR_PATH, &st) == -1) {
        mkdir(LOG_DIR_PATH, LOG_MODE);
    }
    return fopen(filename, "w");
}
