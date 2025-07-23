#include <errno.h>
#include <execinfo.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <eps_cpuinfo.h>
#include <eps_db.h>
#include <eps_efp.h>
#ifdef USE_MQTT
#include <eps_mqtt.h>
#define MQTT_PLUGIN_NAME "mqtt_plugin"
#endif
#include <eps_sem.h>
#include <eps_sem.h>
#include <eps_utils.h>

#include <EMA.h>
#include <EMA/plugins/plugin_mqtt.user.h>

typedef unsigned long long Measurement;
typedef unsigned long long Time;

void crash_handler(int sig) {
    void *buffer[30];
    int nptrs = backtrace(buffer, 30);

    fprintf(stderr, "Caught signal %d (%s)\n", sig, strsignal(sig));
    backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);

    _exit(128 + sig);
}

void install_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; // Reset to default after first signal

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
}

void
efp_main(
    uint32_t jid,
    int nodeid,
    unsigned int gres_uuid_count,
    char** gres_uuid_list,
    time_t tstart,
    const char* cpu_ids,
    eps_cpuinfo_t* cpuinfo
)
{
    int status = EXIT_SUCCESS;
    install_signal_handlers();

    Measurement* e0 = NULL;
    Measurement* e1 = NULL;
    Time* t0 = NULL;
    Time* t1 = NULL;

    sem_t* proceed_init = NULL;
    sem_t* proceed_efp = NULL;
    sem_t* proceed_exit = NULL;

    Device** filtered_devices = NULL;

    char* efp_log_file_path = get_efp_log_file_path(jid);
    FILE* log_fd = get_log_file_fd(efp_log_file_path);
    free(efp_log_file_path);
    if (!log_fd)
    {
        fprintf(stderr, "error: eps: EFP process failed to open log file");
        status = EXIT_FAILURE;
        goto exit;
    }

    if (dup2(fileno(log_fd), STDOUT_FILENO) < 0) {
        perror("dup2 stdout");
    }
    if (dup2(fileno(log_fd), STDERR_FILENO) < 0) {
        perror("dup2 stderr");
    }

    setvbuf(stdout, NULL, _IOLBF, 0);  // Line-buffered
    setvbuf(stderr, NULL, _IONBF, 0);  // Unbuffered

    pid_t efp_pid = getpid();

    printf("EFP PID: %d\n", efp_pid);
    printf("Obtaining semaphores...\n");

    char* sem_init_name = get_sem_init_name(jid);
    proceed_init = get_efp_sem(sem_init_name, 0);
    free(sem_init_name);
    if (!proceed_init) {
        printf("error: get_efp_sem: %s\n", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_efp_name = get_sem_efp_name(jid);
    proceed_efp = get_efp_sem(sem_efp_name, 1);
    free(sem_efp_name);
    if (!proceed_efp) {
        printf("error: get_efp_sem: %s\n", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    char* sem_exit_name = get_sem_exit_name(jid);
    proceed_exit = get_efp_sem(sem_exit_name, 1);
    free(sem_exit_name);
    if (!proceed_exit) {
        printf("error: get_efp_sem: %s\n", strerror(errno));
        status = EXIT_FAILURE;
        goto exit;
    }

    size_t size;
    int* cores = parse_index_range(cpu_ids, &size);
    if (!cores) {
        printf("Failed to parse cpuset restriction!\n");
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
        printf("Utilized on socket %d: %d\n", i, utilized[i]);
        printf(
            "Cores per socket %d: %d\n",
            i,
            cpuinfo->cores_per_socket[i]
        );
        double utilization =
            (double)utilized[i] / (double)cpuinfo->cores_per_socket[i];
        printf("Utilization on socket %d: %f\n", i, utilization);
    }

    printf("Initializig EMA...\n");
    int err;
    #ifdef USE_MQTT
    eps_mqtt_config_t* config = get_mqtt_plugin_config(jid);
    if (config)
    {
      err = register_mqtt_plugin(
        MQTT_PLUGIN_NAME,
        config->host,
        config->port,
        config->topic
      );
      if (err)
      {
          free_mqtt_plugin_config(config);
          printf("Failed to register mqtt plugin: %d\n", err);
          status = EXIT_FAILURE;
          goto exit;
      }
    }
    free_mqtt_plugin_config(config);
    #endif
    err = EMA_init(NULL);
    if (err)
    {
        printf("Failed to initialize EMA: %d\n", err);
        status = EXIT_FAILURE;
        goto exit;
    }

    DevicePtrArray devices = EMA_get_devices();

    // INFO: Release the prolog hook waiting...
    sem_post(proceed_init);

    if (!devices.size) {
        printf("Error: No EMA devices detected!\n");
        status = EXIT_FAILURE;
        goto exit;
    }

    size_t filtered_size = 0;
    // TODO: Impove with realloc ?
    filtered_devices = malloc(devices.size * sizeof(Device*));

    for (int i = 0; i < devices.size; i++)
    {
        Device* dev = devices.array[i];
        const char* name = EMA_get_device_name(dev);
        const char* uuid = EMA_get_device_uid(dev);
        const char* type = EMA_get_device_type(dev);

        if (!gres_uuid_count)
        {
            if (strcmp(type, "cpu") == 0)
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
        if (match || (strcmp(type, "cpu") == 0))
        {
            filtered_devices[filtered_size] = dev;
            filtered_size++;
        }
    }
    #ifdef USE_MQTT
    PluginPtrArray plugins = EMA_get_plugins();
    for (int i = 0; i < plugins.size; i++)
    {
      const char* name = EMA_get_plugin_name(plugins.array[i]);
      if (strcmp(MQTT_PLUGIN_NAME, name) == 0)
      {
        Plugin* mqtt_plugin = plugins.array[i];
        DevicePtrArray mqtt_devices = EMA_get_plugin_devices(mqtt_plugin);
        for (int j = 0; j < mqtt_devices.size; j++)
        {
            filtered_devices[filtered_size] = mqtt_devices.array[j];
            filtered_size++;
        }
        break;
      }
    }
    #endif

    e0 = malloc(filtered_size * sizeof(Measurement));
    e1 = malloc(filtered_size * sizeof(Measurement));
    t0 = malloc(filtered_size * sizeof(Time));
    t1 = malloc(filtered_size * sizeof(Time));

    for (int i = 0; i < filtered_size; i++) {
        printf("Device %d: %s\n", i, EMA_get_device_name(filtered_devices[i]));
        e0[i] = EMA_get_energy_uj(filtered_devices[i]);
        t0[i] = EMA_get_time_in_us();
        printf("\t e0: %llu\n", e0[i]);
        printf("\t t0: %llu\n", t0[i]);
    }

    printf("EFP waiting...\n");

    sem_wait(proceed_efp);

    for (int i = 0; i < filtered_size; i++) {
        e1[i] = EMA_get_energy_uj(filtered_devices[i]);
        t1[i] = EMA_get_time_in_us();
        printf("Device %d: %s\n", i, EMA_get_device_name(filtered_devices[i]));
        printf("\te1: %llu\n", e1[i]);
        printf("\tt1: %llu\n", t1[i]);
    }

    printf("Connecting to db...\n");
    PGconn* db_connection = connect_db();
    int connection_is_not_ok = check_connection(db_connection);
    if (connection_is_not_ok)
    {
        printf(
            "error: problems with db connection: %s\n",
            PQerrorMessage(db_connection)
        );
        PQfinish(db_connection);
        status = EXIT_FAILURE;
        goto exit;
    }

    time_t tend;
    time(&tend);
    // Handle potential error here ?

    char nodename[HOST_NAME_MAX];
    err = gethostname(nodename, HOST_NAME_MAX);
    if (err)
    {
        printf("error: gethostname: %s\n", strerror(errno));
        snprintf(nodename, HOST_NAME_MAX, "Undefined");
    }

    PGresult* res = PQexec(db_connection, "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        printf(
            "error: failed to BEGIN transation:%s\n",
            PQerrorMessage(db_connection)
        );
        PQclear(res);
        PQfinish(db_connection);
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
        printf("error: failed execution data insertion!\n");
        fclose(log_fd);
        PQfinish(db_connection);
        status = EXIT_FAILURE;
        goto exit;
    }

    for (int i = 0; i < filtered_size; i++) {
        const char* device_name = EMA_get_device_name(filtered_devices[i]);
        const char* device_type = EMA_get_device_type(filtered_devices[i]);
        char* device_uid = (char*)EMA_get_device_uid(filtered_devices[i]);

        eps_measurement_data_t measurement;

        double utilization = 100;
        if (strcmp(device_type, "cpu") == 0) {
            char* socket_idx = device_uid;
            int parsed_idx;
            err = eps_parse_int(socket_idx, &parsed_idx);
            if (err) {
                printf(
                    "error: failed to parse socket index: %s\n",
                    socket_idx
                );
                PQfinish(db_connection);
                status = EXIT_FAILURE;
                goto exit;
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
        measurement.device_type = device_type;
        measurement.e0 = e0[i];
        measurement.e1 = e1[i];
        measurement.t0 = t0[i];
        measurement.t1 = t1[i];
        measurement.utilization = utilization;

        err = insert_measurement_data(db_connection, &measurement);
        if (err)
        {
            printf("error: failed measurement data insertion!\n");
            PQfinish(db_connection);
            status = EXIT_FAILURE;
            goto exit;
        }
    }

    res = PQexec(db_connection, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        printf(
            "error: failed to COMMIT transation:%s\n",
            PQerrorMessage(db_connection)
        );
        PQclear(res);
    }

    printf("Closing db connection...\n");
    PQfinish(db_connection);

    printf("Finalizing EMA...\n");
    err = EMA_finalize(NULL);
    if (err)
    {
        printf("Failed to finalize EMA: %d\n", err);
    }
    sem_post(proceed_exit);

exit:
    free(e0);
    free(e1);
    free(t0);
    free(t1);
    free(utilized);

    free(filtered_devices);

    for (int i = 0; i < gres_uuid_count; i++)
    {
      free(gres_uuid_list[i]);
    }
    free(gres_uuid_list);

    printf("Closing semaphores...\n");
    if (proceed_init) sem_close(proceed_init);
    if (proceed_efp) sem_close(proceed_efp);
    if (proceed_exit) sem_close(proceed_exit);

    printf("EFP exiting %d...\n", status);
    if (log_fd) fclose(log_fd);

    exit(status);    
}
