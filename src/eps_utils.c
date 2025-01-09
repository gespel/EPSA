#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <eps_utils.h>

// TODO: Restrict reading for others, currently allowed for testing/debugging
#define LOG_MODE 0644

#define SUFFIX_MAX_LENGTH 15

#define EFP_LOG_PATH_BASE "/tmp/efp_"
#define TINIT_LOG_PATH_BASE "/tmp/task_init_"
#define TEXIT_LOG_PATH_BASE "/tmp/task_exit_"


char* get_suffixed_name(const char* base, int jid) {
    size_t size = strlen(base) + SUFFIX_MAX_LENGTH;
    char* name = calloc(size, sizeof(char));
    snprintf(name, size, "%s%d.log", base, jid);
    return name;
}

char* get_efp_log_file_path(int jid) {
    return get_suffixed_name(EFP_LOG_PATH_BASE, jid);
}

char* get_init_log_file_path(int jid) {
    return get_suffixed_name(TINIT_LOG_PATH_BASE, jid);
}

char* get_exit_log_file_path(int jid) {
    return get_suffixed_name(TEXIT_LOG_PATH_BASE, jid);
}

FILE* get_log_file_fd(const char* filename) {
    unlink(filename);
    return fopen(filename, "w");
}
