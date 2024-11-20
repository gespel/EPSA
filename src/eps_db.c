/*
  Please use the following function to AVOID SQL INJECTION:
    char *PQescapeLiteral(PGconn *conn, const char *str, size_t length);
    char *PQescapeIdentifier(PGconn *conn, const char *str, size_t length);

  https://www.postgresql.org/docs/17/libpq-exec.html#LIBPQ-EXEC-ESCAPE-STRING

*/

#include <eps_db.h>
#include <eps_utils.h>

#include <limits.h>

#define CHECK_PQ_ERR(res, ret) \
    if (PQresultStatus(res) == PGRES_FATAL_ERROR || PQresultStatus(res) != PGRES_COMMAND_OK)\
        return ret;

const char* META_COLS[META_COLS_SIZE] = {
    "job_id", "t_start", "nnodes", "user_id"};
const char* DEVICE_COLS[DEVICE_COLS_SIZE] = {
    "job_id", "nodename", "device", "energy", "t_start", "t_duration"};

PGconn* connect_db()
{
    INFO("Connectiong to db: (%s)...", DB_CONN_INFO);
    return PQconnectdb(DB_CONN_INFO);
}

int check_connection(PGconn* connection)
{
    INFO("Checking db connection (%p)...", connection);
    return PQstatus(connection) != CONNECTION_OK;
}

int create_insert_query(
    char* query,
    PGconn* connection,
    const char* table,
    const char** columns,
    int num_cols,
    char* values
){
    char* _table = PQescapeIdentifier(connection, table, strlen(table));
    int len = 0;
    for(int i=0; i<num_cols; i++)
    {
        char* col = PQescapeIdentifier(
            connection, columns[i], strlen(columns[i])
        );
        len += sizeof(col);
    }
    char* tmp_col = calloc(1, len + 3*num_cols - 1);
    char* _columns = calloc(1, len + 3*num_cols - 1);

    _columns = PQescapeIdentifier(
        connection, columns[0], strlen(columns[0])
    );

    for(int i=1; i<num_cols; i++)
    {
        printf("Escape column %d\n", i);
        char* col = PQescapeIdentifier(
            connection, columns[i], strlen(columns[i])
        );

        sprintf(tmp_col, "%s,%s", _columns, col);
        // PQfreemem(col); // Required or not?
        if(tmp_col != NULL)
            strncpy(_columns, tmp_col, strlen(tmp_col));
        _columns[strlen(tmp_col)] = '\0';

    }

    printf("Build query.\n");
    int num = sprintf(
        query,
        "INSERT INTO %s (%s) VALUES(%s);",
        _table,
        _columns,
        values
    );

    printf("query: %s\n", query);
    PQfreemem(_table);

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
    (PGconn* connection, PGresult** res, const char* table, int jobid)
{
    char query[MAX_QUERY_SIZE];
    char* _table = PQescapeIdentifier(connection, table, strlen(table));
    sprintf(
        query,
        "SELECT * FROM %s WHERE job_id = %d",
        _table,
        jobid
    );

    PQfreemem(_table);

    *res = PQexec(connection, query);

    printf("query: %s\n", query);
    printf("Job ID row: %d\n", PQfnumber(*res, "job_id"));
    int nrows = PQntuples(*res);
    if(nrows == 0)
    {
        PQclear(*res);
        return 1;
    }
    char msg[] = "Found %d entries. Query: %s\n";
    printf(msg, nrows, query);

    return 0;
}

int posix_time_str(char* t_str, size_t buf_size, time_t* time)
{
    struct tm* timeinfo = localtime(time);
    return strftime(t_str, buf_size, "%F %T", timeinfo) == 0;
}

/*********************************** META ************************************/
/* IN connection, data, returns int */
int insert_meta_data(PGconn* connection, eps_meta_data_t* data)
{
    char query[MAX_QUERY_SIZE];
    char values[VALUES_BUFFER_SIZE];

    META_DATA_VALS(connection, data, values);
    printf("Create insert query...\n");
    int err = create_insert_query(
        query, connection, "meta", META_COLS, META_COLS_SIZE , values
    );
    printf("Insert query created.\n");
    if (err) {
        ERROR("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);
    printf("Insert query executed.\n");

    err = check_query_result(result, connection);

    PQclear(result);

    return err;
}

int select_meta_data_by_jobid(
    eps_meta_data_t* data, PGconn* connection, int jobid
){
    PGresult* res = NULL;

    if(row_by_userid(connection, &res, "meta", jobid))
    {
        printf("No entry found!\n");
        return 1;
    }
    data->jobid = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(res, "job_id")), (char **)NULL, 10
    );

    data->userid = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(res, "user_id")), (char **)NULL, 10
    );
    data->nnodes = (int) strtol(
        PQgetvalue(res, 0, PQfnumber(res, "nnodes")), (char **)NULL, 10
    );
    data->tstart = strtol(
        PQgetvalue(res, 0, PQfnumber(res, "t_start")), (char **)NULL, 10
    );
    data->resources = NULL;

    return 0;
}

/********************************** DEVICES **********************************/
/* IN connection, data, returns int */
int insert_device_data(PGconn* connection, eps_device_data_t* data)
{
    char query[MAX_QUERY_SIZE];
    char values[VALUES_BUFFER_SIZE];

    DEVICE_DATA_VALS(connection, data, values);

    int err = create_insert_query(
        query, connection, "devices", DEVICE_COLS, DEVICE_COLS_SIZE, values
    );
    if (err) {
        ERROR("failed to construct query");
        return err;
    }

    PGresult* result = PQexec(connection, query);
    err = check_query_result(result, connection);
    PQclear(result);

    return err;
}

/* Using transaction so insert multiple data in one commit */
int insert_device_data_bulk_ta(
    PGconn* connection, eps_device_data_t* data, int num_data
){
    PGresult* res;

    res = PQexec(connection, "BEGIN");
    CHECK_PQ_ERR(res, 1);

    for(int i=0; i < num_data; i++)
        insert_device_data(connection, &data[i]);

    res = PQexec(connection, "COMMIT");
    CHECK_PQ_ERR(res, 1);

    res = PQexec(connection, "END");
    CHECK_PQ_ERR(res, 1);

    return 0;
}

/*
OUT num_elems
OUT err

For validation num_elems should be equal to nnodes of corresponding entry in
meta table ("job_id" == jobid).
*/
eps_device_data_t* select_device_data_by_jobid(
    int* num_elems, int* err, PGconn* connection, int jobid
){
    PGresult* res;
    eps_device_data_t* data = NULL;

    if(row_by_userid(connection, &res, "devices", jobid))
    {
        printf("No entry found!\n");
        *err = 1;
        return NULL;
    }
    int nrows = PQntuples(res);

    data = calloc(nrows, sizeof(eps_device_data_t));

    for(int i=0; i<nrows; i++)
    {
        data[i].jobid = (int) strtol(
            PQgetvalue(res, i, PQfnumber(res, "job_id")), (char **)NULL, 10
        );
        data[i].nodename = PQgetvalue(res, i, PQfnumber(res, "nodename"));
        data[i].device = PQgetvalue(res, i, PQfnumber(res, "device"));
        data[i].energy = strtoull(
            PQgetvalue(res, i, PQfnumber(res, "energy")), (char **)NULL, 10
        );
        data[i].tstart = strtol(
            PQgetvalue(res, i, PQfnumber(res, "t_start")),
            (char **)NULL,
            10
        );
        data[i].duration = strtoull(
            PQgetvalue(res, i, PQfnumber(res, "t_duration")), (char **)NULL, 10
        );
        // TODO: finalize
        data[i].exclusive = 0;
        data[i].resource = NULL;
        // data->exclusive = strtol(
        //    PQgetvalue(res, i, PQfnumber(res, "exclusive")), (char **)NULL, 10
        //);
        // data->exclusive = PQgetvalue(res, i, PQfnumber(res, "exclusive"));
    }

    *num_elems = nrows;
    *err = 0;

    return data;
}

/*********************************** JOB *************************************/
int compose_job_data(PGconn* connection, eps_job_data_t* job_data, int jobid)
{
    eps_job_data_t* data = calloc(1, sizeof(eps_job_data_t));
    int num_elems;
    long tstart = LONG_MAX;
    unsigned long long duration, end = 0;
    int err = 0;
    eps_device_data_t* devices = select_device_data_by_jobid(
        &num_elems, &err, connection, jobid
    );
    for(int i=0; i<num_elems; i++)
    {
        if(devices[i].jobid != jobid)
            continue;

        data->jobid = devices[i].jobid;

        /* calculate summed energy consumption */
        data->energy += devices[i].energy;

        /* use earliest start time */
        if(devices[i].tstart < tstart)
        {
            data->tstart = devices[i].tstart;
        }
        duration = devices[i].duration;

        /* Use latest end. */
        unsigned long long tmp_end = tstart + duration;
        if(tmp_end > end)
            end = tmp_end;
        /* use duration from ealierst start to latest end */
        data->runtime = data->tstart - end;
        /* use longest measurement duration */
        if(duration > data->duration)
            data->duration = duration;
    }
    return 0;
}
