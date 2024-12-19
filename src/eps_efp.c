#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <eps_db.h>
#include <eps_cgroup.h>
#include <eps_sem.h>
#include <eps_efp.h>
#include <eps_sem.h>
#include <eps_utils.h>

#include <EMA.h>

typedef unsigned long long energy_t;
typedef unsigned long long ustime_t;


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
        LOG(
            msg,
            log_fd,
            "error: get_efp_mutex[%s]: %s",
            sem_name,
            strerror(errno)
        );
        exit(EXIT_FAILURE);
    }
    free(sem_name);

    sem_t* mutex2 = get_efp_mutex(sem_name2, 1);
    if (!mutex2) {
        LOG(
            msg,
            log_fd,
            "error: get_efp_mutex[%s]: %s",
            sem_name2,
            strerror(errno)
        );
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
        energy_t e0[devices.size], e1[devices.size];
        ustime_t t0[devices.size], t1[devices.size];

        for (int i = 0; i < devices.size; i++) {
            e0[i] = EMA_get_energy_uj(devices.array[i]);
            t0[i] = EMA_get_time_in_us();
        }

        LOG(msg, log_fd, "EFP waiting...");
        sem_wait(mutex);

        for (int i = 0; i < devices.size; i++) {
            e1[i] = EMA_get_energy_uj(devices.array[i]);
            t1[i] = EMA_get_time_in_us();
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

        PGresult* res = PQexec(db_connection, "BEGIN");

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            LOG(
                msg,
                log_fd,
                "error: failed to BEGIN transation:%s",
                PQerrorMessage(db_connection)
            );
            PQclear(res);
            PQfinish(db_connection);
            // Clear semaphores here ?
            exit(EXIT_FAILURE);
        }

        eps_execution_data_t execution;
        execution.jobid = jid;
        execution.nodename = nodename;
        execution.nodeid = nodeid;
        execution.tstart = tstart;
        execution.tend = tend;

        int execution_id;

        err = insert_execution_data(db_connection, &execution, &execution_id);
        if (err) {
            LOG(msg, log_fd, "error: failed execution data insertion!");
            //close semaphores ?
            close(log_fd);
            exit(EXIT_FAILURE);
        }

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

            err = insert_measurement_data(db_connection, &measurement);
            if (err) {
                LOG(msg, log_fd, "error: failed measurement data insertion!");
                //close semaphores ?
                close(log_fd);
                exit(EXIT_FAILURE);
            }
        }

        res = PQexec(db_connection, "COMMIT");

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            LOG(
                msg,
                log_fd,
                "error: failed to COMMIT transation:%s",
                PQerrorMessage(db_connection)
            );
            PQclear(res);
        }

        LOG(msg, log_fd, "Closing db connection...");
        PQfinish(db_connection);

        LOG(msg, log_fd, "Finalizing EMA...");
        err = EMA_finalize(NULL);
        if (err) {
            LOG(msg, log_fd, "Failed to finalize EMA: %d", err);
        }

        //INFO: This is important cause slurm will destroy the initial task/step
        //      cgroups at some point and the process will get killed if not moved
        //      from it's initial slurm-created cgroup at this point.
        err = move_pid_to_cg("/sys/fs/cgroup/cgroup.procs", efp_pid);
        if (err) {
            LOG(msg, log_fd, "error: move_pid_to_cg:%s", strerror(errno));
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
