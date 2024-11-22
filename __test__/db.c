#include <eps_data.h>
#include <eps_db.h>

#include <time.h>

#define HANDLE_TEST_ERR(e) do { \
    if(e) \
    { \
        printf("Test failed.\n"); \
        return e; \
    } \
}while(0)

#define CHECK_CONNECTION(conn) do { \
    int err = check_connection(conn); \
    if (err) { \
        printf("Connection error: %s\n", PQerrorMessage(connection));
        PQfinish(conn); \
        return err; \
    } \
}while(0)

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

    exit_on_error(test1(jid), "test1 FAILED!");
}

int test_insert_meta(eps_meta_data_t* data)
{
    printf("Start %s\n", __func__);
    PGconn* conn = PQconnectdb(DB_CONN_INFO);
    CHECK_CONNECTION(conn);

    int err = insert_meta_data(conn, data);
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
    CHECK_CONNECTION(conn);

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
    CHECK_CONNECTION(conn);
    int err = insert_device_data_bulk_ta(conn, data, num_data);
    PQfinish(conn);
    return err;
}

int test_select_device_data(eps_device_data_t* reference)
{
    printf("Start %s\n", __func__);
    eps_device_data_t* data;
    int err = 0;
    int num_devices = 0;
    PGconn* conn;

    conn = PQconnectdb(DB_CONN_INFO);
    CHECK_CONNECTION(conn);
    if(!has_valid_db_entries(conn, reference[0].jobid))
        return 1;

    data = select_device_data_by_jobid(
        &num_devices, &err, conn, reference[0].jobid
    );

    for(int i=0; i<num_devices; i++)
    {
        if(data[i].jobid != reference[i].jobid)
        {
            err = 1;
            printf("`jobid` does not match!\n");
        }
        if(strcmp(data[i].nodename, reference[i].nodename) != 0)
        {
            err = 1;
            printf("`nodename` does not match!\n");
        }
        if(data[i].energy != reference[i].energy)
        {
            err = 1;
            printf("`energy` does not match!\n");
        }
        if(data[i].tstart != reference[i].tstart)
        {
            err = 1;
            printf("`tstart` does not match!\n");
        }
        if(data[i].duration != reference[i].duration)
        {
            err = 1;
            printf("`duration` does not match!\n");
        }
        if(data[i].runtime != reference[i].runtime)
        {
            err = 1;
            printf("`runtime` does not match!\n");
        }
        if(strcmp(data[i].device, reference[i].device) != 0)
        {
            err = 1;
            printf("`device` does not match!\n");
        }
        if(data[i].exclusive != reference[i].exclusive)
        {
            err = 1;
            printf("`exclusive` does not match!\n");
        }
        // TODO: Add check for resources.
    }

    PQfinish(conn);

    return err;
}

int test1(int jobid)
{
    printf("Start %s\n", __func__);
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

    HANDLE_TEST_ERR(test_insert_meta(&meta_data));
    HANDLE_TEST_ERR(test_insert_devices(device_data, num_devices));
    HANDLE_TEST_ERR(test_select_meta_data(&meta_data));
    HANDLE_TEST_ERR(test_select_device_data(device_data));

    print_metadata(&meta_data);
    for(int i=0; i<num_devices;i++)
        print_device_data(&device_data[i]);

    printf("Test was successful!\n");
    return 0;
}
