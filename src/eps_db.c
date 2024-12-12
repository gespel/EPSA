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

#define ALLOC_COLS "job_id, t_start, nnodes, user_id"

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
    uint64_t bin_tstart = htobe64((uint64_t) data->ts);
    uint32_t bin_nnodes = htobe32((uint32_t) data->nnodes);
    uint32_t bin_userid = htobe32((uint32_t) data->userid);

    int paramFormats[4] = {1, 1, 1, 1};
    const char* paramValues[4] = {
        (char*) &bin_jobid,
        (char*) &bin_tstart,
        (char*) &bin_nnodes,
        (char*) &bin_userid
    };
    int paramLengths[4] = {
        sizeof(bin_jobid),
        sizeof(bin_tstart),
        sizeof(bin_nnodes),
        sizeof(bin_userid)
    };

    // TODO: Change the insertion target table to allcations...
    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO meta ("ALLOC_COLS") VALUES($1, $2, $3, $4);",
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
