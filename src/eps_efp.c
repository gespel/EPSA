#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <eps_db.h>
#include <eps_efp.h>
#include <eps_sem.h>
#include <eps_utils.h>

#include <EMA.h>


void efp_main(int jid, int nodeid, time_t tstart) {
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
        sem_close(mutex2);
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


        LOG(msg, log_fd, "Finalizing EMA...");
        err = EMA_finalize(NULL);
        if (err) {
            LOG(msg, log_fd, "Failed to finalize EMA: %d", err);
        }

        LOG(msg, log_fd, "Connecting to db...");
        PGconn* db_connection = connect_db();
        int connection_is_not_ok = check_connection(db_connection);
        if (connection_is_not_ok) {
            LOG(
                msg,
                log_fd,
                "error: problems with db connection: %s",
                PQerrorMessage(db_connection)
            );
            PQfinish(db_connection);
        }

        time_t tend;
        time(&tend);
        // Handle potential error here ?

        char nodename[HOST_NAME_MAX];
        err = gethostname(nodename, HOST_NAME_MAX);
        if (err) {
            LOG(msg, log_fd, "error: gethostname: %s", strerror(errno));
            snprintf(nodename, HOST_NAME_MAX, "Undefined");
        }

        eps_execution_data_t execution;
        execution.jobid = jid;
        execution.nodename = nodename;
        execution.nodeid = nodeid;
        execution.tstart = tstart;
        execution.tend = tend;

        LOG(msg, log_fd, "Execution:");
            LOG(msg, log_fd, "\tjobid: %d", execution.jobid);
            LOG(msg, log_fd, "\tnodename: %s", execution.nodename);
            LOG(msg, log_fd, "\tnodeid: %d", execution.nodeid);
            LOG(msg, log_fd, "\ttstart: %ld", execution.tstart);
            LOG(msg, log_fd, "\ttend: %ld", execution.tend);

        //TODO: Strart transaction...
        //TODO: Make a write to DB and obtain construct execution id...
        int execution_id = 42;

        for (int i = 0; i < devices.size; i++) {
            eps_measurement_data_t measurement;

            measurement.execution_id = execution_id;
            measurement.device_name = EMA_get_device_name(devices.array[i]);
            // TODO: This is temporary, replace with real device uid once implemented
            //       on EMA side...
            measurement.device_uid = "42";
            measurement.e0 = e0[i];
            measurement.e1 = e1[i];
            measurement.t0 = t0[i];
            measurement.t1 = t1[i];

            LOG(msg, log_fd, "Measurement:");
            LOG(msg, log_fd, "\texecution_id: %d", measurement.execution_id);
            LOG(msg, log_fd, "\tdevice_name: %s", measurement.device_name);
            LOG(msg, log_fd, "\tdevice_uid: %s", measurement.device_uid);
            LOG(msg, log_fd, "\te0: %llu", measurement.e0);
            LOG(msg, log_fd, "\te1: %llu", measurement.e1);
            LOG(msg, log_fd, "\tt0: %llu", measurement.t0);
            LOG(msg, log_fd, "\tt1: %llu", measurement.t1);

            //TODO: Write measurement to DB...
        }

        //TODO: Commit transaction...

        LOG(msg, log_fd, "Closing db connection...");
        PQfinish(db_connection);

        free(e0);
        free(e1);
        free(t0);
        free(t1);

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
