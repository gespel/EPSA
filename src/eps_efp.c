#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_utils.h>

#include <EMA.h>

typedef unsigned long long Measurement;
typedef unsigned long long Time;

void efp_main(int jid, unsigned int gres_uuid_count, char** gres_uuid_list) {
    int status = EXIT_SUCCESS;

    Measurement* e0 = NULL;
    Measurement* e1 = NULL;
    Time* t0 = NULL;
    Time* t1 = NULL;

    sem_t* proceed_init = NULL;
    sem_t* proceed_efp = NULL;
    sem_t* proceed_exit = NULL;

    char* efp_log_file_path = get_efp_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(efp_log_file_path);
    free(efp_log_file_path);
    if (!log_fd) {
        fprintf(stderr, "error: eps: EFP process failed to open log file");
        status = EXIT_FAILURE;
        goto exit;
    }

    pid_t efp_pid = getpid();

    LOG(log_fd, "EFP PID: %d", efp_pid);

    LOG(log_fd, "Obtaining semaphores...");

    char* sem_init_name = get_sem_init_name(jid);
    proceed_init = get_efp_sem(sem_init_name, 0);
    free(sem_init_name);
    if (!proceed_init) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_efp_name = get_sem_efp_name(jid);
    proceed_efp = get_efp_sem(sem_efp_name, 1);
    free(sem_efp_name);
    if (!proceed_efp) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_exit_name = get_sem_exit_name(jid);
    proceed_exit = get_efp_sem(sem_exit_name, 1);
    free(sem_exit_name);
    if (!proceed_exit) {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    LOG(log_fd, "Initializig EMA...");
    int err = EMA_init(NULL);

    if (err) {
        LOG(log_fd, "Failed to initialize EMA: %d", err);
        status = EXIT_FAILURE;
        goto exit;
    }

    DevicePtrArray devices = EMA_get_devices();

    // INFO: Release the task_init hook waiting...
    sem_post(proceed_init);

    if (!devices.size) {
        LOG(log_fd, "Error: No EMA devices detected!");
        status = EXIT_FAILURE;
        goto exit;
    }

    size_t filtered_size = 0;
    // TODO: Impove with realloc ?
    Device** filtered_devices = malloc(devices.size * sizeof(Device*));

    for (int i = 0; i < devices.size; i++)
    {
        Device* dev = devices.array[i];
        const char* name = EMA_get_device_name(dev);
        const char* uuid = EMA_get_device_uid(dev);
        char* _name = strdup(name);
        _name[3] = '\0';

        if (!gres_uuid_count)
        {
            if (strcmp(_name, "CPU") == 0)
            {
                filtered_devices[filtered_size] = dev;
                filtered_size++;
            }
            continue;
        }
        int match = 0;
        for (int j = 0; j < gres_uuid_count; j++)
        {
            if (strcmp(uuid, gres_uuid_list[j]) == 0) match = 1;
        }
        if (match || (strcmp(_name, "CPU") == 0))
        {
            filtered_devices[filtered_size] = dev;
            filtered_size++;
        }
    }
    e0 = malloc(filtered_size * sizeof(Measurement));
    e1 = malloc(filtered_size * sizeof(Measurement));
    t0 = malloc(filtered_size * sizeof(Time));
    t1 = malloc(filtered_size * sizeof(Time));

    for (int i = 0; i < filtered_size; i++) {
        LOG(log_fd, "Device %d: %s", i, EMA_get_device_name(filtered_devices[i]));
        e0[i] = EMA_get_energy_uj(filtered_devices[i]);
        t0[i] = EMA_get_time_in_us();
        LOG(log_fd, "\t e0: %llu", e0[i]);
        LOG(log_fd, "\t t0: %llu", t0[i]);
    }

    LOG(log_fd, "EFP waiting...");

    sem_wait(proceed_efp);

    for (int i = 0; i < filtered_size; i++) {
        e1[i] = EMA_get_energy_uj(filtered_devices[i]);
        t1[i] = EMA_get_time_in_us();
        LOG(log_fd, "Device %d: %s", i, EMA_get_device_name(filtered_devices[i]));
        LOG(log_fd, "\te1: %llu", e1[i]);
        LOG(log_fd, "\tt1: %llu", t1[i]);
    }

    LOG(log_fd, "Finalizing EMA...");
    err = EMA_finalize(NULL);
    if (err) {
        LOG(log_fd, "Failed to finalize EMA: %d", err);
    }

    sem_post(proceed_exit);

exit:
    free(e0);
    free(e1);
    free(t0);
    free(t1);

    LOG(log_fd, "Closing semaphores...");
    if (proceed_init) sem_close(proceed_init);
    if (proceed_efp) sem_close(proceed_efp);
    if (proceed_exit) sem_close(proceed_exit);

    LOG(log_fd, "EFP exiting %d...", status);
    if (log_fd) fclose(log_fd);

    exit(status);    
}
