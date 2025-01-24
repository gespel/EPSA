#include <errno.h>
#include <hwloc.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <slurm/spank.h>

#include <eps_cpuinfo.h>
#include <eps_utils.h>
#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_shm.h>

#define PLUGIN_NAME "Spank/Eps"
#define EFP_WAIT_TIMEOUT 10 /* in seconds */

#define CPUINFO_REGION_NAME "/epscpuinfo"


SPANK_PLUGIN(eps, 1)

/********************************
 *
 * Spank functions
 *
 ********************************/
int slurm_spank_init(spank_t sp, int ac, char **av) {
    if (spank_context() == S_CTX_SLURMD) {
        hwloc_topology_t topology;
        hwloc_topology_init(&topology);
        hwloc_topology_load(topology);

        int core_cnt = get_cores_count(topology);
        int sock_cnt = get_sockets_count(topology);

        size_t shm_size = sizeof(int) * (core_cnt + sock_cnt + 2);

        slurm_info("Initializing shared memory...");
        unlink_shared_memory_region(CPUINFO_REGION_NAME);
        int fd = create_shared_memory_region(
            CPUINFO_REGION_NAME,
            shm_size
        );
        if (fd == -1) {
            slurm_info(
                "error: create_shared_memory_region: %s",
                strerror(errno)
            );
            return 1;
        }

        int* mem = map_shared_memory_region(fd, shm_size);

        eps_cpuinfo_t* data = malloc(sizeof(eps_cpuinfo_t));

        int err = populate_cpuinfo(topology, data);
        if (err) {
            hwloc_topology_destroy(topology);
            return 1;
        }

        mem[0] = core_cnt;
        mem[core_cnt + 1] = sock_cnt;

        int* sidx = mem + 1;
        int* cps = mem + core_cnt + 2;

        for(int i = 0; i < data->socket_cnt; i++) {
            cps[i] = data->cores_per_socket[i];
        }

        for(int i = 0; i < core_cnt; i++) {
            sidx[i] = data->socket_idx[i];
        }

        slurm_info("Socket count: %u", mem[core_cnt + 1]);
        for(int i = 0; i < mem[core_cnt + 1]; i++) {
            slurm_info("Cores per socket [%d]: %u", i, cps[i]);
        }

        slurm_info("Cores count: %u", mem[0]);
        for(int i = 0; i < mem[0]; i++) {
            slurm_info("Core [%d] -> Socket [%u]", i, sidx[i]);
        }

        hwloc_topology_destroy(topology);

        unmap_shared_memory_region((void*)mem, shm_size);
        close_shared_memory_region(fd);
    }
    return 0;
}

int slurm_spank_slurmd_exit(spank_t sp, int ac, char **av) {
    unlink_shared_memory_region(CPUINFO_REGION_NAME);
    return 0;
}

int slurm_spank_task_init_privileged(spank_t sp, int ac, char **av) {
    uint32_t jid;

    spank_err_t err = spank_get_item(sp, S_JOB_ID, &jid);
    if (err) {
        return 1;
    }

    char* log_file_path = get_init_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(log_file_path);
    free(log_file_path);
    if (!log_fd) {
        fprintf(stderr, "error: eps: init hook failed to open log file");
        return 1;
    }


    LOG(log_fd, "Initializing shared memory...");
    int shmfd, shmfd_cpuinfo;
    char* shm_name = get_shared_memory_region_name(jid);

    unlink_shared_memory_region(shm_name);
    pid_t* efp_pid = (pid_t*)get_shared_memory_addr(shm_name, sizeof(pid_t*), &shmfd);
    free(shm_name);
    if (!efp_pid) {
        LOG(log_fd, "error: get_shared_memory_addr: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    eps_cpuinfo_t* cpuinfo = (eps_cpuinfo_t*)get_shared_memory_addr(
        CPUINFO_REGION_NAME,
        sizeof(eps_cpuinfo_t),
        &shmfd_cpuinfo
    );
    if (!cpuinfo) {
        LOG(log_fd, "error: get_shared_memory_addr: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_name = get_sem_init_name(jid);
    sem_t* proceed_init = get_efp_sem(sem_name, 1);
    if (!proceed_init) {
        int err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
        if (err) {
            LOG(log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
        }
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    pid_t pid = fork();
    switch(pid) {
        case -1:
            err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                LOG(log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
            }
            sem_close(proceed_init);
            sem_unlink(sem_name);
            free(sem_name);


            LOG(log_fd, "error: fork: %s", strerror(errno));
            fclose(log_fd);
            return 1;
        case 0:
            // Free copied resources after fork...
            err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                LOG(log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
            }
            sem_close(proceed_init);
            free(sem_name);
            fclose(log_fd);

            // INFO: Run EFP process...
            efp_main(jid, cpuinfo);
        default:
            pid_t hook_pid = getpid();

            LOG(log_fd, "Hook PID: %d", hook_pid);

            LOG(log_fd, "Waiting for EFP initialization...");
            sem_wait(proceed_init);

            LOG(log_fd, "Child PID: %d", pid);

            LOG(log_fd, "Writing EFP's PID to shared memory...");
            *efp_pid = pid;

            err =  discard_shared_memory_addr((void*)efp_pid, sizeof(pid_t*), &shmfd);
            if (err) {
                LOG(log_fd, "error: discard_shared_memory_addr: %s", strerror(errno));
            }

            LOG(log_fd, "Closing semaphore...");
            sem_close(proceed_init);
            sem_unlink(sem_name);
            free(sem_name);

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

    char* shm_name = get_shared_memory_region_name(jid);
    
    char* log_file_path = get_exit_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(log_file_path);
    free(log_file_path);
    if (!log_fd) {
        unlink_shared_memory_region(shm_name);
        free(shm_name);
        fprintf(stderr, "error: eps: exit hook failed to open log file");
        return 1;
    }

    pid_t hook_pid = getpid();

    LOG(log_fd, "Hook PID: %d", hook_pid);

    LOG(log_fd, "Obtaining shared memory address...");
    int shmfd = open_shared_memory_region(shm_name);
    if (shmfd == -1) {
        LOG(log_fd, "error: open_shared_memory_region: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    pid_t* efp_pid = (pid_t*)map_shared_memory_region(shmfd, sizeof(pid_t*));
    if (!efp_pid) {
        close_shared_memory_region(shmfd);
        LOG(log_fd, "error: map_shared_memory_region: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_efp_name = get_sem_efp_name(jid);
    sem_t* resume_efp = get_efp_sem(sem_efp_name, 0);
    if (!resume_efp) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);

        free(sem_efp_name);

        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    char* sem_exit_name = get_sem_exit_name(jid);
    sem_t* efp_finalize = get_efp_sem(sem_exit_name, 0);
    if (!efp_finalize) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);

        free(sem_efp_name);
        free(sem_exit_name);

        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        return 1;
    }

    LOG(log_fd, "Unlocking semaphore (resuming EFP)...");
    sem_post(resume_efp);

    LOG(log_fd, "Waiting for EFP to finish or fail...");
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        unmap_shared_memory_region((void*)efp_pid, sizeof(pid_t*));
        close_shared_memory_region(shmfd);
        unlink_shared_memory_region(shm_name);
        free(shm_name);

        sem_close(resume_efp);
        sem_close(efp_finalize);

        sem_unlink(sem_efp_name);
        sem_unlink(sem_exit_name);

        free(sem_efp_name);
        free(sem_exit_name);

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
    unlink_shared_memory_region(shm_name);
    free(shm_name);

    fclose(log_fd);

    return 0;
}
