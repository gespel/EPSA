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

    char* sem_name = get_sem_name(jid);

    sem_t* mutex = get_efp_mutex(sem_name, 1);
    if (!mutex) {
        LOG(msg, log_fd, "error: get_efp_mutex: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    free(sem_name);

    pid_t child_pid = getpid();

    LOG(msg, log_fd, "EFP PID: %d", child_pid);

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
        unsigned long long* consumptions =
            (unsigned long long*)calloc(devices.size, sizeof(unsigned long long));

        for (int i = 0; i < devices.size; i++) {
            LOG(msg, log_fd, "Device %d: %s", i, EMA_get_device_name(devices.array[i]));
            e0[i] = EMA_get_energy_uj(devices.array[i]);
        }

        LOG(msg, log_fd, "Child waiting...");

        sem_wait(mutex);

        LOG(msg, log_fd, "Consumptions:");
        for (int i = 0; i < devices.size; i++) {
            e1[i] = EMA_get_energy_uj(devices.array[i]);
            consumptions[i] = e1[i] - e0[i];
            LOG(msg, log_fd, "\t%s: %llu", EMA_get_device_name(devices.array[i]), consumptions[i]);
        }

        free(e0);
        free(e1);
        free(consumptions);

        LOG(msg, log_fd, "Finalizing EMA...");
        err = EMA_finalize(NULL);
        if (err) {
            LOG(msg, log_fd, "Failed to finalize EMA: %d", err);
        }

        LOG(msg, log_fd, "Child exiting success...");

        sem_close(mutex);
        close(log_fd);

        exit(EXIT_SUCCESS);
    } else {
        LOG(msg, log_fd, "Error: No EMA devices detected!");
        sem_close(mutex);
        close(log_fd);
        exit(EXIT_FAILURE);
    }
}
