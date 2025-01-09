#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include <slurm/spank.h>

#include <eps_efp.h>
#include <eps_sem.h>
#include <eps_shm.h>
#include <eps_utils.h>

#define PLUGIN_NAME "Spank/Eps"
#define EFP_WAIT_TIMEOUT 10 /* in seconds */


SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    char msg[LOG_MSG_BUFF_SIZE];
    time_t tstart;
    uint32_t jid;

    spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
    if (err) {
        return 1;
    }

    char* log_file_path = get_init_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(log_file_path);
    free(log_file_path);

    time(&tstart);
    if (tstart == -1) {
        LOG(msg, log_fd, "error: time: %s", strerror(errno));
        return 1;
    }

    pid_t hook_pid = getpid();

    LOG(log_fd, "Hook PID: %d", hook_pid);

    LOG(log_fd, "Initializing shared memory...");
    int shmfd;
    char* shm_name = get_shared_memory_region_name(jid);

    unlink_shared_memory_region(shm_name);
    pid_t* efp_pid = (pid_t*)get_shared_memory_addr(shm_name, sizeof(pid_t*), &shmfd);
    free(shm_name);
    if (!efp_pid) {
        LOG(log_fd, "error: get_shared_memory_addr: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_name = get_sem_init_name(jid);
    sem_t* proceed_init = get_efp_sem(sem_name, 1);
    if (!proceed_init) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    uint32_t nodeid = 0;
    err = spank_get_item(sp, S_JOB_NODEID, &nodeid);
    if (err) {
        LOG(
            msg, log_fd, 
            "error: spank_get_item[nodeid]: %s",
            spank_strerror(errno)
        );
    }

    LOG(msg, log_fd, "Node ID: %d", nodeid);

    pid_t pid = fork();
    switch(pid) {
        case -1:
            LOG(log_fd, "error: fork: %s", strerror(errno));
            fclose(log_fd);
            return 1;
        case 0:
            efp_main(jid, nodeid, tstart);
        default:
            LOG(log_fd, "Waiting for EFP initialization...");
            sem_wait(proceed_init);

            LOG(log_fd, "Child PID: %d", pid);

            LOG(log_fd, "Writing EFP's PID to shared memory...");
            *efp_pid = pid;

            int err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                LOG(log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
            }

            LOG(log_fd, "Init hook exits...");
            fclose(log_fd);
            return 0;
    }
    return 0;
}

int slurm_spank_task_exit(spank_t sp, int ac, char **av) {
    uint32_t jid;

    spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
    if (err) {
        return 1;
    }
    
    char* log_file_path = get_exit_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(log_file_path);
    free(log_file_path);

    char msg[LOG_MSG_BUFF_SIZE];

    pid_t hook_pid = getpid();

    LOG(log_fd, "Hook PID: %d", hook_pid);

    LOG(log_fd, "Obtaining shared memory address...");
    char* shm_name = get_shared_memory_region_name(jid);

    int shmfd = open_shared_memory_region(shm_name);
    free(shm_name);
    if (shmfd == -1) {
        LOG(log_fd, "error: open_shared_memory_region: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    pid_t* efp_pid = (pid_t*)map_shared_memory_region(shmfd, sizeof(pid_t*));
    if (!efp_pid) {
        LOG(log_fd, "error: map_shared_memory_region: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_efp_name = get_sem_efp_name(jid);
    sem_t* resume_efp = get_efp_sem(sem_efp_name, 0);
    if (!resume_efp) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    char* sem_exit_name = get_sem_exit_name(jid);
    sem_t* efp_finalize = get_efp_sem(sem_exit_name, 0);
    if (!efp_finalize) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Unlocking semaphore (resuming EFP)...");
    sem_post(resume_efp);

    LOG(log_fd, "Waiting for EFP to finish or fail...");
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        LOG(log_fd, "error: clock_gettime: %s", strerror(errno));
        fclose(log_fd);
        // Should we retrun here or use sem_trywait ?
        return 1;
    }

    ts.tv_sec += EFP_WAIT_TIMEOUT;
    int ret = sem_timedwait(efp_finalize, &ts);
    if (ret == -1 && errno == ETIMEDOUT) {
        LOG(log_fd, "error: efp timed out!");
        kill(*efp_pid, 9);
    }

    LOG(log_fd, "Closing semaphores...");
    sem_close(resume_efp);
    sem_close(efp_finalize);

    sem_unlink(sem_efp_name);
    sem_unlink(sem_exit_name);

    free(sem_efp_name);
    free(sem_exit_name);

    LOG(log_fd, "Cleaning up shared memory...");
    unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
    close_shared_memory_region(shmfd);

    fclose(log_fd);

    return 0;
}
