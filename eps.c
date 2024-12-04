#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <slurm/spank.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

const char* task_init_log_file = "/tmp/task_init.log";

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

void run_child_process() {
    char msg[256];

    pid_t child_pid = getpid();
    pid_t parent_pid = getppid();

    sprintf(msg, "Child PID: %d", child_pid);
    log_message(msg, task_init_log_file);

    sprintf(msg, "Parent PID: %d", parent_pid);
    log_message(msg, task_init_log_file);

    // INFO: Currently if the sleep is there, you will not see
    //       the child exit log. Probably is is because the forked
    //       process appears in the process group of slurmstepd and 
    //       when it exits, it kills all spawned processes in that group.
    //       this is just my assumption by now, it is kinda hard to
    //       tell for sure...
    //sprintf(msg, "Child sleeping...");
    //log_message(msg, task_init_log_file);
    //sleep(3);

    sprintf(msg, "Child exiting success...");
    log_message(msg, task_init_log_file);
    exit(EXIT_SUCCESS);
}

/********************************
 *
 * Spank functions
 *
 ********************************/

int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    pid_t hook_pid = getpid();
    char msg[256];

    remove_log_file(task_init_log_file);

    sprintf(msg, "Hook PID: %d", hook_pid);
    log_message(msg, task_init_log_file);

    sprintf(msg, "Hook Cgroup:");
    log_message(msg, task_init_log_file);
    log_cgroup(hook_pid, task_init_log_file);

    pid_t pid = fork();

    switch(pid) {
        case -1:
            sprintf(msg, "error: fork: %s", strerror(errno));
            log_message(msg, task_init_log_file);
            return 1;
        case 0:
            run_child_process();
        default:
            sprintf(msg, "Child PID: %d", pid);
            log_message(msg, task_init_log_file);

            sprintf(msg, "Hook Cgroup (After fork):");
            log_message(msg, task_init_log_file);
            log_cgroup(hook_pid, task_init_log_file);

            sprintf(msg, "Child Cgroup (After fork):");
            log_message(msg, task_init_log_file);
            log_cgroup(pid, task_init_log_file);

            sprintf(msg, "Init hook exits...");
            log_message(msg, task_init_log_file);
            return 0;
    }

    return 0;
}

int slurm_spank_init(spank_t sp, int ac, char **av) {
    slurm_info("Init: " PLUGIN_NAME);
    return 0;
}

int slurm_spank_exit(spank_t sp, int ac, char **av) {
    slurm_info("Exit: " PLUGIN_NAME);
    return 0;
}

int slurm_spank_job_prolog (spank_t sp, int ac, char **av) {
    slurm_info("Prolog: " PLUGIN_NAME);
    return 0;
}

int slurm_spank_job_epilog (spank_t sp, int ac, char **av) {
    slurm_info("Epilog: " PLUGIN_NAME);
    return 0;
}
