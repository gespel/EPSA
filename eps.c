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
#define EFP_EXIT_DELAY 500000 /* in microseconds */


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
    int log_fd = get_log_file_fd(log_file_path);
    free(log_file_path);

    time(&tstart);
    if (tstart == -1) {
        LOG(msg, log_fd, "error: time: %s", strerror(errno));
        return 1;
    }

    pid_t hook_pid = getpid();

    LOG(msg, log_fd, "Hook PID: %d", hook_pid);

    LOG(msg, log_fd, "Initializing shared memory...");
    int shmfd;
    char* shm_name = get_shared_memory_region_name(jid);

    unlink_shared_memory_region(shm_name);
    pid_t* efp_pid = (pid_t*)get_shared_memory_addr(shm_name, sizeof(pid_t*), &shmfd);
    if (!efp_pid) {
        LOG(msg, log_fd, "error: get_shared_memory_addr: %s", strerror(errno));
        return 1;
    }
    free(shm_name);

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
            LOG(msg, log_fd, "error: fork: %s", strerror(errno));
            return 1;
        case 0:
            efp_main(jid, nodeid, tstart);
        default:
            LOG(msg, log_fd, "Child PID: %d", pid);

            LOG(msg, log_fd, "Writing EFP's PID to shared memory...");
            *efp_pid = pid;

            int err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                LOG(msg, log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
            }

            LOG(msg, log_fd, "Init hook exits...");
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

    char msg[LOG_MSG_BUFF_SIZE];

    pid_t hook_pid = getpid();

    LOG(msg, log_fd, "Hook PID: %d", hook_pid);

    LOG(msg, log_fd, "Obtaining shared memory address...");
    char* shm_name = get_shared_memory_region_name(jid);

    int shmfd = open_shared_memory_region(shm_name);
    if (shmfd == -1) {
        LOG(msg, log_fd, "error: open_shared_memory_region: %s", strerror(errno));
        // To return or not to return ???
    }
    free(shm_name);

    pid_t* efp_pid = (pid_t*)map_shared_memory_region(shmfd, sizeof(pid_t*));
    if (!efp_pid) {
        LOG(msg, log_fd, "error: map_shared_memory_region: %s", strerror(errno));
        // To return or not to return ???
    }

    LOG(msg, log_fd, "Obtaining semaphores...");
    char* sem_name = get_sem_name(jid);
    char* sem_name2 = get_sem2_name(jid);

    sem_t* mutex = get_efp_mutex(sem_name, 0);
    if (!mutex) {
        LOG(msg, log_fd, "error: get_efp_mutex[%s]: %s",sem_name, strerror(errno));
        return 1;
    }

    sem_t* mutex2 = get_efp_mutex(sem_name2, 0);
    if (!mutex2) {
        LOG(msg, log_fd, "error: get_efp_mutex[%s]: %s",sem_name2, strerror(errno));
        return 1;
    }

    LOG(msg, log_fd, "Unlocking semaphore (resuming EFP)...");
    sem_post(mutex);

    LOG(msg, log_fd, "Waiting for EFP to finish or fail...");
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        LOG(msg, log_fd, "error: clock_gettime: %s", strerror(errno));
        // Should we retrun here or use sem_trywait ?
        return 1;
    }

    ts.tv_sec += EFP_WAIT_TIMEOUT;
    int ret = sem_timedwait(mutex2, &ts);
    if (ret == -1 && errno == ETIMEDOUT) {
        LOG(msg, log_fd, "error: efp timed out!");
        kill(*efp_pid, 9);
    } else {
        // INFO: Give EFP some time to write last logs and exit...
        usleep(EFP_EXIT_DELAY);
    }

    LOG(msg, log_fd, "Closing semaphores...");
    sem_close(mutex);
    sem_close(mutex2);

    sem_unlink(sem_name);
    sem_unlink(sem_name2);

    free(sem_name);
    free(sem_name2);

    LOG(msg, log_fd, "Cleaning up shared memory...");
    unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
    close_shared_memory_region(shmfd);

    close(log_fd);

    return 0;
}
