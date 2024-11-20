#include <eps_data.h>
#include <eps_db.h>

#include <time.h>

#define T_NOW(X) \
    char X[20]; \
    time_t now;\
    time(&now);\
    struct tm *local_time = localtime(&now); \
    do{\
        sprintf(\
            X, \
            "%04d-%02d-%02d %02d:%02d:%02d", \
            local_time->tm_year+1900, \
            local_time->tm_mon, \
            local_time->tm_mday, \
            local_time->tm_hour, \
            local_time->tm_min, \
            local_time->tm_sec \
        );\
    } while(0)

PGconn* connection = NULL;

int test1(int jobid);

void exit_on_error(int ret, const char* msg)
{
    if(ret)
    {
        printf("%s: %s\n", msg, PQerrorMessage(connection));
        PQfinish(connection);
        exit(1);
    }
}

int main(int argc, char** argv){
    if(argc != 2)
    {
        printf("Add programm argument: %s <jobid>\n", __FILE__);
        return 0;
    }
    int jid = (int) strtol(argv[1], (char **)NULL, 10);

    /* Connecting DB. */
    connection = PQconnectdb(DB_CONN_INFO);
    printf("DB connected.\n");

    /* Checking DB connection. */
    int err = check_connection(connection);
    if (err) {
        exit_on_error(err, "Connection check failed!");
    }

    eps_meta_data_t data;

    err = select_meta_data_by_jobid(&data, connection, jid);
    int testable = 0;
    if (err) {
        testable = 1;
        // free_metadata(&data);
        // exit_on_error(err, "Failed to read metadata from db!");
    }else{
        // free_metadata(&data);
        print_metadata(&data);
    }

    /* Disconnecting DB. */
    PQfinish(connection);
    printf("DB disconnected.\n");

    if(testable)
    {
        err = test1(jid);
        if(err)
            exit_on_error(err, "Failed test1!");
    }
}

int test_insert_meta(eps_meta_data_t* data)
{
    printf("Start %s\n", __func__);
    PGconn* conn = PQconnectdb(DB_CONN_INFO);
    int err = check_connection(conn);
    if (err) {
        printf("Connection check failed!");
        return err;
    }
    printf("insert_meta_data...\n");
    err = insert_meta_data(conn, data);
    printf("insert_meta_data done.\n");

    if(err)
        printf("Test failed. Could not insert meta data.\n");

    PQfinish(conn);

    return err;
}

int test_select_meta_data(eps_meta_data_t* reference)
{
    printf("Start %s\n", __func__);
    eps_meta_data_t data;
    int err = 0;
    PGconn* conn = PQconnectdb(DB_CONN_INFO);
    err = check_connection(conn);
    if (err) {
        printf("Connection check failed!");
        return err;
    }

    select_meta_data_by_jobid(&data, conn, reference->jobid);
    if(data.jobid != reference->jobid)
    {
        err = 1;
        printf("Test failed. `jobid` does not match!\n");
    }
    if(data.userid != reference->userid)
    {
        err = 1;
        printf("Test failed. `userid` does not match!\n");
    }
    if(data.nnodes != reference->nnodes)
    {
        err = 1;
        printf("Test failed. `nnodes` does not match!\n");
    }
    if(data.tstart != reference->tstart)
    {
        err = 1;
        printf("Test failed. `tstart` does not match! ( %ld vs. %ld )\n", data.tstart, reference->tstart);
    }
    // if(data.resources = reference->ressources)
    //     printf("Test failed. `resources` does not match!\n")

    PQfinish(conn);

    return err;
}

int test_insert_devices(eps_device_data_t* data, int num_data)
{
    printf("Start %s\n", __func__);
    PGconn* conn = PQconnectdb(DB_CONN_INFO);
    int err = check_connection(conn);
    if(err) {
        printf("Connection check failed!");
        return err;
    }

    err = insert_device_data_bulk_ta(conn, data, num_data);

    PQfinish(conn);

    return err;
}

int test_select_device_data(eps_device_data_t* reference)
{
    printf("Start %s\n", __func__);
    eps_device_data_t* data;
    int err = 0;
    PGconn* conn = PQconnectdb(DB_CONN_INFO);
    err = check_connection(conn);
    if (err) {
        printf("Connection check failed!");
        return err;
    }

    int num_devices;
    data = select_device_data_by_jobid(
        &num_devices, &err, conn, reference[0].jobid
    );
    for(int i=0; i<num_devices; i++)
    {
        if(data[i].jobid != reference[i].jobid)
        {
            err = 1;
            printf("Test failed. `jobid` does not match!\n");
        }
        if(strcmp(data[i].nodename, reference[i].nodename) != 0)
        {
            err = 1;
            printf("Test failed. `nodename` does not match!\n");
        }
        if(data[i].energy != reference[i].energy)
        {
            err = 1;
            printf("Test failed. `energy` does not match!\n");
        }

        if(data[i].tstart != reference[i].tstart)
        {
            // err = 1;
            printf(
                "Test failed. `tstart` does not match (%ld != %ld)!\n",
                data[i].tstart, reference[i].tstart
            );
        }
        if(data[i].duration != reference[i].duration)
        {
            err = 1;
            printf("Test failed. `duration` does not match!\n");
        }
        if(data[i].runtime != reference[i].runtime)
        {
            err = 1;
            printf("Test failed. `runtime` does not match! (%lld != %lld)\n", data[i].runtime, reference[i].runtime);
        }
        if(strcmp(data[i].device, reference[i].device) != 0)
        {
            err = 1;
            printf("Test failed. `device` does not match!\n");
        }
        // if(strcmp(data[i].resource, reference[i].resource) != 0)
        // {
        //     err = 1;
        //     printf("Test failed. `resource` does not match!\n");
        // }
        if(data[i].exclusive != reference[i].exclusive)
        {
            // err = 1;
            printf("Test failed. `exclusive` does not match! (%d != %d) \n", data[i].exclusive, reference[i].exclusive);
        }
    // char* device;
    // void* device_type; /* EMA plugin type? */
    // // TODO: Clarify what this should be...
    // void* resource;
    // int exclusive;
    }
    PQfinish(conn);

    return err;
}

int test1(int jobid)
{
    printf("Start %s\n", __func__);
    int err = 0;

    time_t t_now = time(NULL);
    eps_meta_data_t meta_data = {
        .jobid = jobid,
        .userid = 123,
        .nnodes = 2,
        .tstart = t_now,
        .resources = NULL,
    };
    int num_devices = 2;
    eps_device_data_t device_data[] = {
        {
            .jobid = jobid,
            .nodename = "node0",
            .energy = 876543210,
            .tstart = t_now,
            .duration = 123350,
            .runtime = 0,
            .device = "CPU-0",
            .resource = NULL,
            .exclusive = 0,
        },
        {
            .jobid = jobid,
            .nodename = "node1",
            .energy = 123456789,
            .tstart = t_now,
            .duration = 123450,
            .runtime = 0,
            .device = "CPU-0",
            .resource = NULL,
            .exclusive = 0,
        }
    };

    err = test_insert_meta(&meta_data);
    if(err)
    {
        printf("Test failed.\n");
        return err;
    }
    err = test_insert_devices(device_data, num_devices);
    if(err)
    {
        printf("Test failed.\n");
        return err;
    }

    err = test_select_meta_data(&meta_data);
    if(err)
    {
        printf("Test failed.\n");
        return err;
    }
    err = test_select_device_data(device_data);
    if(err)
    {
        printf("Test failed.\n");
        return err;
    }

    printf("test was successful\n");

    print_metadata(&meta_data);
    for(int i=0; i<num_devices;i++)
        print_device_data(&device_data[i]);
    return err;
}
