#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <eps_cgroup.h>
#include <eps_cpuinfo.h>
#include <eps_db.h>
#include <eps_efp.h>
#include <eps_sem.h>
#include <eps_sem.h>
#include <eps_utils.h>

#include <EMA.h>

typedef unsigned long long Measurement;
typedef unsigned long long Time;


void
efp_main(
    int jid,
    int nodeid,
    time_t tstart,
    const char* cpu_ids,
    eps_cpuinfo_t* cpuinfo
)
{
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
    if (!log_fd)
    {
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
    if (!proceed_init)
    {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_efp_name = get_sem_efp_name(jid);
    proceed_efp = get_efp_sem(sem_efp_name, 1);
    free(sem_efp_name);
    if (!proceed_efp)
    {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_exit_name = get_sem_exit_name(jid);
    proceed_exit = get_efp_sem(sem_exit_name, 1);
    free(sem_exit_name);
    if (!proceed_exit)
    {
        LOG(log_fd, "error: get_efp_sem: %s", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    size_t size;
    int* cores = parse_cpuset_restriction(cpu_ids, &size);
    if (!cores) {
        LOG(log_fd, "Failed to parse cpuset restriction!");
        status = EXIT_FAILURE;
        goto exit;
    }

    int* utilized = calloc(cpuinfo->socket_cnt, sizeof(int));

    for (int i = 0; i < size; i++) {
        int cidx = cores[i];
        int sidx = cpuinfo->socket_idx[cidx];
        utilized[sidx]++;
    }

    for (int i = 0; i < cpuinfo->socket_cnt; i++) {
        LOG(log_fd, "Utilized on socket %d: %d", i, utilized[i]);
        LOG(
            log_fd,
            "Cores per socket %d: %d",
            i,
            cpuinfo->cores_per_socket[i]
        );
        double utilization =
            (double)utilized[i] / (double)cpuinfo->cores_per_socket[i];
        LOG(log_fd, "Utilization on socket %d: %f", i, utilization);
    }

    LOG(log_fd, "Initializig EMA...");
    int err = EMA_init(NULL);

    if (err)
    {
        LOG(log_fd, "Failed to initialize EMA: %d", err);
        status = EXIT_FAILURE;
        goto exit;
    }

    DevicePtrArray devices = EMA_get_devices();

    // INFO: Release the task_init hook waiting...
    sem_post(proceed_init);

    if (!devices.size)
    {
        LOG(log_fd, "Error: No EMA devices detected!");
        status = EXIT_FAILURE;
        goto exit;
    }

    e0 = malloc(devices.size * sizeof(Measurement));
    e1 = malloc(devices.size * sizeof(Measurement));
    t0 = malloc(devices.size * sizeof(Time));
    t1 = malloc(devices.size * sizeof(Time));

    for (int i = 0; i < devices.size; i++)
    {
        e0[i] = EMA_get_energy_uj(devices.array[i]);
        t0[i] = EMA_get_time_in_us();
    }

    LOG(log_fd, "EFP waiting...");

    sem_wait(proceed_efp);

    for (int i = 0; i < devices.size; i++)
    {
        e1[i] = EMA_get_energy_uj(devices.array[i]);
        t1[i] = EMA_get_time_in_us();
    }

    LOG(log_fd, "Connecting to db...");
    PGconn* db_connection = connect_db();
    int connection_is_not_ok = check_connection(db_connection);
    if (connection_is_not_ok)
    {
        LOG(
            log_fd,
            "error: problems with db connection: %s",
            PQerrorMessage(db_connection)
        );
        PQfinish(db_connection);
        // Clear semaphores here and exit failure ?
    }

    time_t tend;
    time(&tend);
    // Handle potential error here ?

    char nodename[HOST_NAME_MAX];
    err = gethostname(nodename, HOST_NAME_MAX);
    if (err)
    {
        LOG(log_fd, "error: gethostname: %s", strerror(errno));
        snprintf(nodename, HOST_NAME_MAX, "Undefined");
    }

    PGresult* res = PQexec(db_connection, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        LOG(
            log_fd,
            "error: failed to BEGIN transation:%s",
            PQerrorMessage(db_connection)
        );
        PQclear(res);
        PQfinish(db_connection);
        // Clear semaphores here ?
        status = EXIT_FAILURE;
        goto exit;
    }

    eps_execution_data_t execution;
    execution.jobid = jid;
    execution.nodename = nodename;
    execution.nodeid = nodeid;
    execution.tstart = tstart;
    execution.tend = tend;

    int execution_id;

    err = insert_execution_data(db_connection, &execution, &execution_id);
    if (err)
    {
        LOG(log_fd, "error: failed execution data insertion!");
        fclose(log_fd);
        status = EXIT_FAILURE;
        goto exit;
    }

    for (int i = 0; i < devices.size; i++) {
        const char* device_name = EMA_get_device_name(devices.array[i]);
        char* device_uid = (char*)EMA_get_device_uid(devices.array[i]);

        eps_measurement_data_t measurement;

        double utilization = 100;
        if (strstr(device_uid, "CPU-")) {
            char* socket_idx = device_uid + 4;
            int parsed_idx;
            err = eps_parse_int(socket_idx, &parsed_idx);
            if (err) {
                LOG(
                    log_fd,
                    "error: failed to parse socket index: %s",
                    socket_idx
                );
                // TODO: Decide what to do here...
            } else {
                utilization =
                    (double)utilized[parsed_idx] /
                    (double)cpuinfo->cores_per_socket[parsed_idx];
                utilization = utilization * (double)100;
            }
        }

        measurement.execution_id = execution_id;
        measurement.device_name = device_name;
        measurement.device_uid = device_uid;
        measurement.e0 = e0[i];
        measurement.e1 = e1[i];
        measurement.t0 = t0[i];
        measurement.t1 = t1[i];
        measurement.utilization = utilization;

        err = insert_measurement_data(db_connection, &measurement);
        if (err)
        {
            LOG(log_fd, "error: failed measurement data insertion!");
            status = EXIT_FAILURE;
            goto exit;
        }
    }

    res = PQexec(db_connection, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        LOG(
            log_fd,
            "error: failed to COMMIT transation:%s",
            PQerrorMessage(db_connection)
        );
        PQclear(res);
    }

    LOG(log_fd, "Closing db connection...");
    PQfinish(db_connection);

    LOG(log_fd, "Finalizing EMA...");
    err = EMA_finalize(NULL);
    if (err)
    {
        LOG(log_fd, "Failed to finalize EMA: %d", err);
    }
    sem_post(proceed_exit);

exit:
    free(e0);
    free(e1);
    free(t0);
    free(t1);

    free(utilized);

    LOG(log_fd, "Closing semaphores...");
    if (proceed_init) sem_close(proceed_init);
    if (proceed_efp) sem_close(proceed_efp);
    if (proceed_exit) sem_close(proceed_exit);

    LOG(log_fd, "EFP exiting %d...", status);
    if (log_fd) fclose(log_fd);

    exit(status);    
}
