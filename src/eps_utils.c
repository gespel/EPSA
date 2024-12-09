#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_utils.h>


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
