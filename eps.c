#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <slurm/spank.h>

#include <eps_utils.h>
#include <eps_sem.h>
#include <eps_efp.h>

#define PLUGIN_NAME "Spank/Eps"

SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    uint32_t jid;

    spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
    if (err) {
        return 1;
    }

    char* log_file_path = get_init_log_file_path(jid);
    int log_fd = get_log_file_fd(log_file_path);

    pid_t hook_pid = getpid();
    pid_t hook_pgid = getpgid(hook_pid);
    pid_t hook_sid = getsid(hook_pid);

    char msg[128];

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
            efp_main(hook_pgid, jid);
        default:
            sprintf(msg, "Child PID: %d\n", pid);
            log_message(msg, log_fd);

            sprintf(msg, "Child Process GID: %d\n", getpgid(pid));
            log_message(msg, log_fd);

            sprintf(msg, "Init hook exits...\n");
            log_message(msg, log_fd);
            return 0;
    }

    free(log_file_path);
    close(log_fd);

    return 0;
}

int slurm_spank_task_exit(spank_t sp, int ac, char **av) {
    uint32_t jid;

    spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
    if (err) {
        return 1;
    }
    
    char* log_file_path = get_exit_log_file_path(jid);
    int log_fd = get_log_file_fd(log_file_path);
    char* sem_name = get_sem_name(jid);

    char msg[128];

    sem_t* mutex = get_efp_mutex(sem_name, 0);
    if (!mutex) {
        sprintf(msg, "error: get_efp_mutex: %s", strerror(errno));
        log_message(msg, log_fd);
        return 1;
    }

    int sem_val;
    sem_getvalue(mutex, &sem_val);
    sprintf(msg, "/efpsem: %d\n", sem_val);
    log_message(msg, log_fd);

    sem_post(mutex);

    sem_getvalue(mutex, &sem_val);
    sprintf(msg, "/efpsem: %d\n", sem_val);
    log_message(msg, log_fd);

    pid_t hook_pid = getpid();
    pid_t hook_pgid = getpgid(hook_pid);
    pid_t hook_sid = getsid(hook_pid);

    sprintf(msg, "Hook PID: %d\n", hook_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Hook SID: %d\n", hook_sid);
    log_message(msg, log_fd);

    sprintf(msg, "Hook Process GID: %d\n", hook_pgid);
    log_message(msg, log_fd);

    sem_unlink(sem_name);
    free(log_file_path);
    free(sem_name);
    close(log_fd);

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
