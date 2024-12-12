#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_utils.h>

// TODO: Restrict reading for others, currently allowed for testing/debugging
#define LOG_MODE 0644

#define SUFFIX_MAX_LENGTH 15

#define EFP_LOG_PATH_BASE "/tmp/efp_"
#define TINIT_LOG_PATH_BASE "/tmp/task_init_"
#define TEXIT_LOG_PATH_BASE "/tmp/task_exit_"


void remove_log_file(const char* log_file) {
    char cmd[256];
    sprintf(cmd,"rm %s", log_file);
    system(cmd);
}

void log_message(const char* message, int fd) {
    if (fd <= 0) return;
    write(fd,message, strlen(message));
}

void log_cgroup(pid_t pid, const char* log_file) {
    char cmd[256];
    sprintf(cmd, "cat /proc/%d/cgroup >> %s", pid, log_file);
    system(cmd);
}

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

int get_log_file_fd(const char* filename) {
    int filename_length = strlen(filename);

    // INFO: Remove file if exists...
    // TODO: Imrpove this, currently it uglifies the output of srun,
    //       should be a way to check for file existence first, and omit this
    //       invocation...
    char cmd[filename_length + 4];
    sprintf(cmd, "rm %s", filename);
    system(cmd);

    int fd = open(filename, O_RDWR | O_CREAT, LOG_MODE);
    if (fd == -1) perror("open");

    return fd;
}
