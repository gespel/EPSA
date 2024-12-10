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
        LOG(msg, log_fd, "error: get_efp_mutex: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    free(sem_name);

    pid_t child_pid = getpid();

    LOG(msg, log_fd, "EFP PID: %d\n", child_pid);

    LOG(msg, log_fd, "Initializig EMA...\n");
    int err = EMA_init(NULL);

    if (err) {
        LOG(msg, log_fd, "Failed to initialize EMA: %d\n", err);
        exit(EXIT_FAILURE);
    }

    DevicePtrArray devices = EMA_get_devices();

    if (!devices.size) {
        LOG(msg, log_fd, "Error: No EMA devices detected!\n");
    } else {
        for (int i = 0; i < devices.size; i++) {
            LOG(msg, log_fd, "Device %d: %s\n", i, EMA_get_device_name(devices.array[i]));
        }
    }

    LOG(msg, log_fd, "Child waiting...\n");

    sem_wait(mutex);

    LOG(msg, log_fd, "Finalizing EMA...\n");
    err = EMA_finalize(NULL);
    if (err) {
        LOG(msg, log_fd, "Failed to finalize EMA: %d\n", err);
    }

    LOG(msg, log_fd, "Child exiting success...\n");

    sem_close(mutex);
    close(log_fd);

    exit(EXIT_SUCCESS);
}
