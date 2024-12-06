#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <slurm/spank.h>

#include <eps_utils.h>
#include <eps_efp.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

const char* task_init_log_file = "/tmp/task_init.log";
const char* task_exit_log_file = "/tmp/task_exit.log";

/********************************
 *
 * Spank functions
 *
 ********************************/

int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    pid_t hook_pid = getpid();
    pid_t hook_pgid = getpgid(hook_pid);
    pid_t hook_sid = getsid(hook_pid);
    char msg[256];

    remove_log_file(task_init_log_file);

    sprintf(msg, "Hook PID: %d", hook_pid);
    log_message(msg, task_init_log_file);

    sprintf(msg, "Hook SID: %d", hook_sid);
    log_message(msg, task_init_log_file);

    sprintf(msg, "Hook Process GID: %d", hook_pgid);
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
            efp_main(hook_pgid);
        default:
            sprintf(msg, "Child PID: %d", pid);
            log_message(msg, task_init_log_file);

            sprintf(msg, "Child Process GID: %d", getpgid(pid));
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

int slurm_spank_task_exit(spank_t sp, int ac, char **av) {
    pid_t hook_pid = getpid();
    pid_t hook_pgid = getpgid(hook_pid);
    pid_t hook_sid = getsid(hook_pid);
    char msg[256];

    remove_log_file(task_exit_log_file);

    sprintf(msg, "Hook PID: %d", hook_pid);
    log_message(msg, task_exit_log_file);

    sprintf(msg, "Hook SID: %d", hook_sid);
    log_message(msg, task_exit_log_file);

    sprintf(msg, "Hook Process GID: %d", hook_pgid);
    log_message(msg, task_exit_log_file);

    sprintf(msg, "Hook Cgroup:");
    log_message(msg, task_exit_log_file);
    log_cgroup(hook_pid, task_exit_log_file);

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
