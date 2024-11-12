#include <eps_db.h>
#include <eps_utils.h>

extern PGconn* db_connection;

int connect_db()
{
    db_connection = PQconnectdb(DB_CONN_INFO);
    if (
        !db_connection
        || (PQstatus(db_connection) != CONNECTION_OK)
    )
    {
        slurm_error(
            "db connection failed: %s",
            PQerrorMessage(db_connection)
        );
        return 1;
    }

    return 0;
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
    slurm_info("Query: %s\n", query);

    return num <= 0;
}

int check_query_result(PGresult* result, PGconn* connection)
{
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        slurm_error(
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
        slurm_error("failed to construct query");
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
        slurm_error("failed to construct query");
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
        slurm_error("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);

    err = check_query_result(result, connection);

    PQclear(result);

    return err;
}

int select_device_data_by_jobid(
    eps_device_data_t* data, PGconn* db_conn, int jobid
){
    char query[MAX_QUERY_SIZE];
    PGresult* res;

    sprintf(query, "SELECT * FROM devices WHERE job_id = '%d'", jobid);
    res = PQexec(db_conn, query);

    if(row_by_userid(db_conn, res, "devices", jobid))
        return 1;

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
    data->nodename = PQgetvalue(res, 0, PQfnumber(res, "nodename"));
    data->device = PQgetvalue(res, 0, PQfnumber(res, "device"));
    data->energy = strtoull(
        PQgetvalue(res, 0, PQfnumber(res, "energy")), (char **)NULL, 10
    );

    data->tstart = PQgetvalue(res, 0, PQfnumber(res, "t_start"));
    data->duration = strtoull(
        PQgetvalue(res, 0, PQfnumber(res, "t_duration")), (char **)NULL, 10
    );

    // TODO: finalize
    data->device_type = NULL;
    data->exclusive = 0;
    data->resource = NULL;
    // data->device_type = PQgetvalue(res, 0, PQfnumber(res, "device_type"));
    // data->exclusive = strtol(
    //    PQgetvalue(res, 0, PQfnumber(res, "exclusive")), (char **)NULL, 10
    //);
    // data->exclusive = PQgetvalue(res, 0, PQfnumber(res, "exclusive"));

    return 0;
}
