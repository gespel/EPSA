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

    remove_log_file(task_init_log_file);
    int log_fd = open(task_init_log_file, O_RDWR | O_CREAT, 0644);
    char msg[128];




    log_cgroup(hook_pid, task_init_log_file);
    sprintf(msg, "Hook PID: %d\n", hook_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Hook SID: %d\n", hook_sid);
    log_message(msg, log_fd);
    sprintf(msg, "Hook Process GID: %d\n", hook_pgid);
    log_message(msg, log_fd);
    pid_t pid = fork();

    switch(pid) {
        case -1:
            sprintf(msg, "error: fork: %s\n", strerror(errno));
            log_message(msg, log_fd);
            return 1;
        case 0:
            efp_main(hook_pgid);
        default:
            sprintf(msg, "Child PID: %d\n", pid);
            log_message(msg, log_fd);

            sprintf(msg, "Child Process GID: %d\n", getpgid(pid));
            log_message(msg, log_fd);

            //sprintf(msg, "Hook Cgroup (After fork):\n");
            //log_message(msg, log_fd);
            //log_cgroup(hook_pid, task_init_log_file);
            //sprintf(msg, "Child Cgroup (After fork):\n");
            //log_message(msg, log_fd);
            //log_cgroup(pid, task_init_log_file);
            sprintf(msg, "Init hook exits...\n");
            log_message(msg, log_fd);
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

    int exit_fd = open(task_exit_log_file, O_RDWR | O_CREAT, 0644);
    

    sprintf(msg, "Global char: %p\n", task_init_log_file);
    log_message(msg, exit_fd);

    sprintf(msg, "Global int: %p\n", &test);
    log_message(msg, exit_fd);

    sprintf(msg, "Global char value: %s\n", task_init_log_file);
    log_message(msg, exit_fd);

    sprintf(msg, "Global int value: %d\n", test);
    log_message(msg, exit_fd);

    sprintf(msg, "Hook PID: %d\n", hook_pid);
    log_message(msg, exit_fd);

    sprintf(msg, "Hook SID: %d\n", hook_sid);
    log_message(msg, exit_fd);

    sprintf(msg, "Hook Process GID: %d\n", hook_pgid);
    log_message(msg, exit_fd);

    //sprintf(msg, "Hook Cgroup:\n");
    //log_message(msg, exit_fd);
    //log_cgroup(hook_pid, task_exit_log_file);

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
