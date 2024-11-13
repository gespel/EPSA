#include <eps_db.h>
#include <eps_utils.h>

PGconn* connect_db()
{
    return PQconnectdb(DB_CONN_INFO);
}

int check_connection(PGconn* connection)
{
    return PQstatus(connection) != CONNECTION_OK ? 1 : 0;
}

//TODO: Consider better name e.g. compose_query, create_query etc.
int insert(
    char* query,
    const char* table,
    const char* columns,
    char* values
){
    int num = sprintf(
        query,
        "INSERT INTO %s (%s) VALUES(%s);",
        table,
        columns,
        values
    );

    return num <= 0;
}

int check_query_result(PGresult* result, PGconn* connection)
{
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        ERROR(
            "eps-db: failed to execute query: %s\n",
            PQerrorMessage(connection)
        );
        return 1;
    }
    return 0;
}

int row_by_userid
    (PGconn* db_conn, PGresult* res, const char* tablename, int jobid)
{
    char query[MAX_QUERY_SIZE];

    sprintf(query, "SELECT * FROM %s WHERE job_id = '%d'", tablename, jobid);
    res = PQexec(db_conn, query);

    printf("User ID row: %d\n", PQfnumber(res, "user_id"));
    int nrows = PQntuples(res);
    if(nrows != 1)
    {
        char msg[] = "Single entry expected. Found %d entries. Query: %s";
        printf(msg, nrows, query);
        PQclear(res);
        return 1;
    }

    return 0;
}
/*********************************** META ************************************/
/* IN db_conn, data, returns int */
int insert_meta_data(PGconn* connection, eps_meta_data_t* data)
{
    char query[MAX_QUERY_SIZE];
    char values[VALUES_BUFFER_SIZE];

    META_DATA_VALS(data, values);

    int err = insert(query, "meta", META_COLS, values);
    if (err) {
        ERROR("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);

    err = check_query_result(result, connection);

    PQclear(result);

    return err;
}

int select_meta_data_by_jobid(eps_meta_data_t* data, PGconn* db_conn, int jobid)
{
    PGresult* res = NULL;
    PGresult* row = NULL;

    if(row_by_userid(db_conn, row, "meta", jobid))
        return 1;

    data->jobid = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(row, "job_id")), (char **)NULL, 10
    );
    data->userid = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(row, "user_id")), (char **)NULL, 10
    );
    data->nnodes = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(row, "nnodes")), (char **)NULL, 10
    );
    data->tstart = PQgetvalue(res, 0, PQfnumber(row, "t_start"));
    data->resources = NULL;

    return 0;
}

/************************************ JOB ************************************/
/* IN db_conn, data, returns int */
int insert_job_data(PGconn* connection, eps_job_data_t* data)
{
    char query[MAX_QUERY_SIZE];
    char values[VALUES_BUFFER_SIZE];

    JOB_DATA_VALS(data, values);

    int err = insert(query, "jobs", JOB_COLS, values);
    if (err) {
        ERROR("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);

    err = check_query_result(result, connection);

    PQclear(result);

    return err;
}

int select_job_data_by_jobid(eps_job_data_t* data, PGconn* db_conn, int jobid)
{
    char query[MAX_QUERY_SIZE];
    PGresult* res;

    sprintf(query, "SELECT * FROM jobs WHERE job_id = '%d'", jobid);
    res = PQexec(db_conn, query);

    printf("User ID row: %d\n", PQfnumber(res, "user_id"));
    int nrows = PQntuples(res);
    if(nrows != 1)
    {
        char msg[] = "Single entry expected. Found %d entries. Query: %s";
        printf(msg, nrows, query);
        PQclear(res);
        return 1;
    }

    data->jobid = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(res, "job_id")), (char **)NULL, 10
    );
    data->energy = strtoull(
        PQgetvalue(res, 0, PQfnumber(res, "energy")), (char **)NULL, 10
    );
    data->duration = strtoull(
        PQgetvalue(res, 0, PQfnumber(res, "duration")), (char **)NULL, 10
    );
    data->tstart = PQgetvalue(res, 0, PQfnumber(res, "t_start"));

    return 0;
}
/********************************** DEVICES **********************************/
/* IN db_conn, data, returns int */
int insert_device_data(PGconn* connection, eps_device_data_t* data)
{
    char query[MAX_QUERY_SIZE];
    char values[VALUES_BUFFER_SIZE];

    DEVICE_DATA_VALS(data, values);

    int err = insert(query, "devices", DEVICE_COLS, values);
    if (err) {
        ERROR("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);

    err = check_query_result(result, connection);

    PQclear(result);

    return err;
}

/*
OUT num_elems
OUT err
*/
eps_device_data_t** select_device_data_by_jobid(
    int* num_elems, int err, PGconn* db_conn, int jobid
){
    char query[MAX_QUERY_SIZE];
    PGresult* res;
    eps_device_data_t** data = NULL;
    sprintf(
        query,
        "SELECT * FROM devices WHERE job_id = '%d' EXTRACT(EPOCH FROM T_START) As unix_timestamp;",
        jobid
    );
    res = PQexec(db_conn, query);

    if(row_by_userid(res, "devices", jobid))
    {
        err = 1;
        return NULL;
    }

    int nrows = PQntuples(res);
    if(nrows < 1)
    {
        char msg[] = "No entry found. Query: %s";
        printf(msg, query);
        PQclear(res);
        err = 1;
        return NULL;
    }

    data = calloc(nrows, sizeof(eps_device_data_t));

    for(int i=0; i<nrows; i++;)
    {
        data[i]->jobid = (int) strtol(
            PQgetvalue(res, i, PQfnumber(res, "job_id")), (char **)NULL, 10
        );
        data[i]->nodename = PQgetvalue(res, i, PQfnumber(res, "nodename"));
        data[i]->device = PQgetvalue(res, i, PQfnumber(res, "device"));
        data[i]->energy = strtoull(
            PQgetvalue(res, i, PQfnumber(res, "energy")), (char **)NULL, 10
        );
        data[i]->tstart = PQgetvalue(res, i, PQfnumber(res, "t_start"));
        data[i]->duration = strtoull(
            PQgetvalue(res, i, PQfnumber(res, "t_duration")
        );
        // TODO: finalize
        data[i]->device_type = NULL;
        data[i]->exclusive = 0;
        data[i]->resources = NULL;
        // data->device_type = PQgetvalue(res, i, PQfnumber(res, "device_type"));
        // data->exclusive = strtol(
        //    PQgetvalue(res, i, PQfnumber(res, "exclusive")), (char **)NULL, 10
        //);
        // data->exclusive = PQgetvalue(res, i, PQfnumber(res, "exclusive"));
        data[i]->tstart_posix = strtol(
            PQgetvalue(res, i, PQfnumber(res, "unix_timestamp")
        );
    }

    return data;
}

int compose_job_data(eps_device_data_t** device_data, int num_elems, int jobid)
{
    eps_job_data_t* data = calloc(1, eps_job_data_t);
    int jobid;
    char* tstart;
    unsigned long long duration, end, energy = 0;
    for(int i=0; i<num_elems; i++;)
    {
        jobid = (int) strtol(
            PQgetvalue(res, i, PQfnumber(res, "job_id")), (char **)NULL, 10
        );
        if(data->jobid != jobid)
            continue;

        data->jobid = jobid;

        /* calculate summed energy consumption */
        data->energy += strtoull(
            PQgetvalue(res, i, PQfnumber(res, "energy")), (char **)NULL, 10
        );

        tstart = PQgetvalue(res, i, PQfnumber(res, "t_start"));
        /* use ealiest start time */
        if(tstart_posix < data->tstart_posix)
        {
            data->tstart_posix = tstart_posix;
            data->tstart = tstart;
        }
        duration = strtoull(PQgetvalue(res, i, PQfnumber(res, "t_duration"));
        /* Use latest end. */
        unsigned long long tmp_end = tstart_posix + duration
        if(tmp_end > end)
            end = tmp_end;
        /* use duration from ealierst start to latest end */
        data->runtime = data->tstart_posix - end;
        /* use longest measurement duration */
        if(duration > data->duration)
            data->duration = duration;
    }
}
