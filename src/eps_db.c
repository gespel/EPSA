/*
  Please use the following function to AVOID SQL INJECTION:
    PGresult *PQexecParams(PGconn *conn,
                       const char *command,
                       int nParams,
                       const Oid *paramTypes,
                       const char * const *paramValues,
                       const int *paramLengths,
                       const int *paramFormats,
                       int resultFormat);
    char *PQescapeLiteral(PGconn *conn, const char *str, size_t length);
    char *PQescapeIdentifier(PGconn *conn, const char *str, size_t length);

For further details check:
- https://www.postgresql.org/docs/17/libpq-exec.html#LIBPQ-PQEXECPARAMS
- https://www.postgresql.org/docs/17/libpq-exec.html#LIBPQ-EXEC-ESCAPE-STRING
*/

#include <eps_db.h>

#define CHECK_PQ_ERR(res, ret) \
    if (PQresultStatus(res) == PGRES_FATAL_ERROR || PQresultStatus(res) != PGRES_COMMAND_OK)\
        return ret;

#define ALLOC_COLS "jobid,  nnodes, userid, ts"
#define EXEC_COLS "jobid,  node_name, node_id, ts_start, ts_end"

PGconn* connect_db()
{
    // WARN: This is usecure to print whole connection string!
    // TODO: Find a secure way to log connection info (perhaps partially e.g.
    //       only the host address), or remove the connection info from this log
    //       completely.
    return PQconnectdb(DB_CONN_INFO);
}

int check_connection(PGconn* connection)
{
    return PQstatus(connection) != CONNECTION_OK;
}

int check_query_result(PGresult* result, PGconn* connection)
{
    ExecStatusType status = PQresultStatus(result);
    int status_ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    if (!status_ok) {
        printf(
            "eps-db: failed to execute query[%s]: %s\n",
            PQresStatus(PQresultStatus(result)),
            PQerrorMessage(connection)
        );
        return 1;
    }
    return 0;
}

int row_by_userid
    (PGconn* connection, PGresult** res, const char* table, int jobid)
{
    char* query = malloc(33 + strlen(table) + 1);
    sprintf(query, "SELECT * FROM %s WHERE job_id = $1;",
        PQescapeIdentifier(connection, table, strlen(table))
    );

    uint32_t bin_jobid = htobe32((uint32_t) jobid);
    const char* paramValues[1] = {(char*) &bin_jobid};
    int paramLengths[1] = {sizeof(bin_jobid)};
    int paramFormats[1] = {1};

    *res = PQexecParams(
        connection,
        query,
        1,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        1
    );

    check_query_result(*res, connection);

    int nrows = PQntuples(*res);
    if(nrows == 0)
    {
        PQclear(*res);
        return 1;
    }

    printf("Found %d entries in table %s with jobid=%d.", nrows, table, jobid);

    return 0;
}

int posix_time_str(char* t_str, size_t buf_size, time_t* time)
{
    struct tm* timeinfo = localtime(time);
    return strftime(t_str, buf_size, "%F %T", timeinfo) == 0;
}

/*********************************** ALLOCATION ************************************/
/* IN connection, data, returns int */
int insert_allocation_data(PGconn* connection, eps_allocation_data_t* data)
{
    uint32_t bin_jobid = htobe32((uint32_t) data->jobid);
    uint32_t bin_nnodes = htobe32((uint32_t) data->nnodes);
    uint32_t bin_userid = htobe32((uint32_t) data->userid);

    char ts[12];
    snprintf(ts, 12, "%ld", data->ts);

    int paramFormats[4] = {1, 1, 1, 0};
    const char* paramValues[4] = {
        (char*) &bin_jobid,
        (char*) &bin_nnodes,
        (char*) &bin_userid,
        ts
    };
    int paramLengths[4] = {
        sizeof(bin_jobid),
        sizeof(bin_nnodes),
        sizeof(bin_userid),
        sizeof(ts)
    };

    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO allocations ("ALLOC_COLS") VALUES($1, $2, $3, to_timestamp($4));",
        4,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        0
    );
    int err = check_query_result(res, connection);

    PQclear(res);

    return err;
}

/*********************************** EXECUTION ************************************/
/* IN connection, data, returns int */
int insert_execution_data(PGconn* connection, eps_execution_data_t* data, int* id)
{
    uint32_t bin_jobid = htobe32((uint32_t) data->jobid);
    uint32_t bin_nodeid = htobe32((uint32_t) data->nodeid);

    char tstart[12];
    snprintf(tstart, 12, "%ld", data->tstart);

    char tend[12];
    snprintf(tend, 12, "%ld", data->tend);

    int paramFormats[5] = {1, 0, 1, 0, 0};
    const char* paramValues[5] = {
        (char*) &bin_jobid,
        data->nodename,
        (char*) &bin_nodeid,
        tstart,
        tend
    };
    int paramLengths[5] = {
        sizeof(bin_jobid),
        sizeof(data->nodename),
        sizeof(bin_nodeid),
        sizeof(tstart),
        sizeof(tend)
    };

    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO executions ("EXEC_COLS") VALUES($1, $2, $3, to_timestamp($4), to_timestamp($5)) RETURNING id;",
        5,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        0
    );
    int err = check_query_result(res, connection);
    if (err) return err;

    for (int i = 0; i < PQntuples(res); i++) {
      for (int j = 0; j < PQnfields(res); j++) {
        *id = strtol(PQgetvalue(res, i, j), NULL, 10);
      }
    }

    PQclear(res);

    return 0;

