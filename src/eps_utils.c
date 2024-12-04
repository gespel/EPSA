#include <stdio.h>
#include <stdlib.h>

#include <eps_utils.h>


void remove_log_file(const char* log_file) {
    char cmd[256];
    sprintf(cmd,"rm %s", log_file);
    system(cmd);
}

void log_message(const char* message, const char* log_file) {
    char cmd[1024];
    sprintf(cmd, "echo '%s' >> %s", message, log_file);
    system(cmd);
}

void log_cgroup(pid_t pid, const char* log_file) {
    char cmd[256];
    sprintf(cmd, "cat /proc/%d/cgroup >> %s", pid, log_file);
    system(cmd);
}
