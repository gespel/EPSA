#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_utils.h>

#include <EMA.h>


void efp_main(int jid) {
    char msg[256];

    char* efp_log_file_path = get_efp_log_file_path(jid);
    int log_fd = get_log_file_fd(efp_log_file_path);
    free(efp_log_file_path);

    pid_t efp_pid = getpid();

    LOG(msg, log_fd, "EFP PID: %d", efp_pid);

    LOG(msg, log_fd, "Obtaining semaphores...");
    char* sem_name = get_sem_name(jid);
    char* sem_name2 = get_sem2_name(jid);

    sem_t* mutex = get_efp_mutex(sem_name, 1);
    if (!mutex) {
        LOG(msg, log_fd, "error: get_efp_mutex: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    free(sem_name);

    sem_t* mutex2 = get_efp_mutex(sem_name2, 1);
    if (!mutex2) {
        LOG(msg, log_fd, "error: get_efp_mutex: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    free(sem_name2);

    LOG(msg, log_fd, "Initializig EMA...");
    int err = EMA_init(NULL);

    if (err) {
        LOG(msg, log_fd, "Failed to initialize EMA: %d", err);
        sem_close(mutex);
        close(log_fd);
        exit(EXIT_FAILURE);
    }

    DevicePtrArray devices = EMA_get_devices();

    if (devices.size) {
        unsigned long long* e0 =
            (unsigned long long*)calloc(devices.size, sizeof(unsigned long long));
        unsigned long long* e1 =
            (unsigned long long*)calloc(devices.size, sizeof(unsigned long long));

        unsigned long long* t0 =
            (unsigned long long*)calloc(devices.size, sizeof(unsigned long long));
        unsigned long long* t1 =
            (unsigned long long*)calloc(devices.size, sizeof(unsigned long long));

        for (int i = 0; i < devices.size; i++) {
            LOG(msg, log_fd, "Device %d: %s", i, EMA_get_device_name(devices.array[i]));
            e0[i] = EMA_get_energy_uj(devices.array[i]);
            t0[i] = EMA_get_time_in_us();
            LOG(msg, log_fd, "\t e0: %llu", e0[i]);
            LOG(msg, log_fd, "\t t0: %llu", t0[i]);
        }

        LOG(msg, log_fd, "EFP waiting...");

        sem_wait(mutex);

        for (int i = 0; i < devices.size; i++) {
            e1[i] = EMA_get_energy_uj(devices.array[i]);
            t1[i] = EMA_get_time_in_us();
            LOG(msg, log_fd, "Device %d: %s", i, EMA_get_device_name(devices.array[i]));
            LOG(msg, log_fd, "\te1: %llu", e1[i]);
            LOG(msg, log_fd, "\tt1: %llu", t1[i]);
        }

        free(e0);
        free(e1);
        free(t0);
        free(t1);

        LOG(msg, log_fd, "Finalizing EMA...");
        err = EMA_finalize(NULL);
        if (err) {
            LOG(msg, log_fd, "Failed to finalize EMA: %d", err);
        }

        sem_post(mutex2);

        LOG(msg, log_fd, "Closing semaphores...");
        sem_close(mutex);
        sem_close(mutex2);

        LOG(msg, log_fd, "EFP exiting success...");
        close(log_fd);

        exit(EXIT_SUCCESS);
    } else {
        LOG(msg, log_fd, "Error: No EMA devices detected!");

        sem_post(mutex2);

        LOG(msg, log_fd, "Closing semaphores...");
        sem_close(mutex);
        sem_close(mutex2);

        LOG(msg, log_fd, "EFP exiting failure...");
        close(log_fd);

        exit(EXIT_FAILURE);
    }
}
