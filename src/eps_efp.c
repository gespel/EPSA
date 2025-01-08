#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eps_cgroup.h>
#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_utils.h>

#include <EMA.h>


void efp_main(int jid) {
    char msg[256];

    char* efp_log_file_path = get_efp_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(efp_log_file_path);
    free(efp_log_file_path);

    pid_t efp_pid = getpid();

    LOG(log_fd, "EFP PID: %d", efp_pid);

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_name = get_sem_name(jid);
    sem_t* proceed_init = get_efp_sem(sem_name, 0);
    free(sem_name);
    if (!proceed_init) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        exit(EXIT_FAILURE);
    }

    char* sem_name2 = get_sem2_name(jid);
    sem_t* proceed_exit = get_efp_sem(sem_name2, 1);
    free(sem_name2);
    if (!proceed_exit) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        fclose(log_fd);
        exit(EXIT_FAILURE);
    }

    LOG(log_fd, "Initializig EMA...");
    int err = EMA_init(NULL);

    if (err) {
        LOG(log_fd, "Failed to initialize EMA: %d", err);
        sem_close(proceed_init);
        sem_close(proceed_exit);
        fclose(log_fd);
        exit(EXIT_FAILURE);
    }

    DevicePtrArray devices = EMA_get_devices();

    //INFO: This is important cause slurm will destroy the initial task/step
    //      cgroups at some point and the process will get killed if not moved
    //      from it's initial slurm-created cgroup at this point.
    err = move_pid_to_cg("/sys/fs/cgroup/cgroup.procs", efp_pid);
    if (err) {
        LOG(log_fd, "error: move_pid_to_cg:%s", strerror(errno));
    }

    // INFO: Release the task_init hook waiting...
    sem_post(proceed_init);

    if (devices.size) {
        unsigned long long e0[devices.size], e1[devices.size];
        unsigned long long t0[devices.size], t1[devices.size];

        for (int i = 0; i < devices.size; i++) {
            LOG(log_fd, "Device %d: %s", i, EMA_get_device_name(devices.array[i]));
            e0[i] = EMA_get_energy_uj(devices.array[i]);
            t0[i] = EMA_get_time_in_us();
            LOG(log_fd, "\t e0: %llu", e0[i]);
            LOG(log_fd, "\t t0: %llu", t0[i]);
        }

        LOG(log_fd, "EFP waiting...");

        sem_wait(proceed_init);

        for (int i = 0; i < devices.size; i++) {
            e1[i] = EMA_get_energy_uj(devices.array[i]);
            t1[i] = EMA_get_time_in_us();
            LOG(log_fd, "Device %d: %s", i, EMA_get_device_name(devices.array[i]));
            LOG(log_fd, "\te1: %llu", e1[i]);
            LOG(log_fd, "\tt1: %llu", t1[i]);
        }

        LOG(log_fd, "Finalizing EMA...");
        err = EMA_finalize(NULL);
        if (err) {
            LOG(log_fd, "Failed to finalize EMA: %d", err);
        }

        sem_post(proceed_exit);

        LOG(log_fd, "Closing semaphores...");
        sem_close(proceed_init);
        sem_close(proceed_exit);

        LOG(log_fd, "EFP exiting success...");
        fclose(log_fd);

        exit(EXIT_SUCCESS);
    } else {
        LOG(log_fd, "Error: No EMA devices detected!");

        sem_post(proceed_exit);

        LOG(log_fd, "Closing semaphores...");
        sem_close(proceed_init);
        sem_close(proceed_exit);

        LOG(log_fd, "EFP exiting failure...");
        fclose(log_fd);

        exit(EXIT_FAILURE);
    }
}
