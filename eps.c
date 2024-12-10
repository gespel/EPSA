#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <slurm/spank.h>

#include <eps_utils.h>
#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_shm.h>

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
    free(log_file_path);

    pid_t hook_pid = getpid();
    pid_t hook_pgid = getpgid(hook_pid);

    char msg[LOG_MSG_BUFF_SIZE];

    sprintf(msg, "Hook PID: %d\n", hook_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Initializing shared memory...\n");
    log_message(msg, log_fd);
    int shmfd;
    char* shm_name = get_shared_memory_region_name(jid);

    unlink_shared_memory_region(shm_name);
    pid_t* efp_pid = (pid_t*)get_shared_memory_addr(shm_name, sizeof(pid_t*), &shmfd);
    if (!efp_pid) {
        sprintf(msg, "error: get_shared_memory_addr: %s\n", strerror(errno));
        log_message(msg, log_fd);
        return 1;
    }
    free(shm_name);

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

            sprintf(msg, "Writing EFP's PID to shared memory\n");
            log_message(msg, log_fd);
            *efp_pid = pid;

            int err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                sprintf(msg, "error: discard_shared_memory_addr: %s\n", strerror(errno));
                log_message(msg, log_fd);
            }

            sprintf(msg, "Init hook exits...\n");
            log_message(msg, log_fd);
            return 0;
    }

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
    free(log_file_path);
    char* sem_name = get_sem_name(jid);

    char msg[LOG_MSG_BUFF_SIZE];

    pid_t hook_pid = getpid();

    sprintf(msg, "Hook PID: %d\n", hook_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Obtaining shared memory address...\n");
    log_message(msg, log_fd);
    char* shm_name = get_shared_memory_region_name(jid);

    int shmfd = open_shared_memory_region(shm_name);
    if (shmfd == -1) {
        sprintf(msg, "error: open_shared_memory_region: %s", strerror(errno));
        log_message(msg, log_fd);
        // To return or not to return ???
    }
    free(shm_name);

    pid_t* efp_pid = (pid_t*)map_shared_memory_region(shmfd, sizeof(pid_t*));
    if (!efp_pid) {
        sprintf(msg, "error: map_shared_memory_region: %s", strerror(errno));
        log_message(msg, log_fd);
        // To return or not to return ???
    }

    sprintf(msg, "Reading shared memory...\n");
    log_message(msg, log_fd);
    sprintf(msg, "EFP's PID: %d...\n", *efp_pid);
    log_message(msg, log_fd);

    sprintf(msg, "Obtaining semaphore...\n");
    log_message(msg, log_fd);
    sem_t* mutex = get_efp_mutex(sem_name, 0);
    if (!mutex) {
        sprintf(msg, "error: get_efp_mutex: %s", strerror(errno));
        log_message(msg, log_fd);
        return 1;
    }

    sprintf(msg, "Unlocking semaphore...\n");
    log_message(msg, log_fd);
    sem_post(mutex);

    sprintf(msg, "Closing semaphore...\n");
    log_message(msg, log_fd);
    sem_close(mutex);
    sem_unlink(sem_name);
    free(sem_name);

    sprintf(msg, "Waiting for EFP to finish or fail...\n");
    log_message(msg, log_fd);
    while(1) {
        int ret = kill(*efp_pid, 0);
        if (ret == -1 && errno == ESRCH) break;
        usleep(500);
    }

    sprintf(msg, "Cleaning up shared memory...\n");
    log_message(msg, log_fd);
    unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
    close_shared_memory_region(shmfd);

    close(log_fd);

    return 0;
}
