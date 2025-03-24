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

#include <unistd.h>

#include <eps_db.h>

#define ALLOC_COLS "jobid, job_name, nnodes, userid, ts"
#define EXEC_COLS "jobid, node_name, node_id, ts_start, ts_end"
#define MES_COLS "exec_id, device_name, device_uid, e0, e1, t0, t1"

PGconn* connect_db()
{
    char* conn_str = getenv("EPS_DB_CONN_STR");
    return PQconnectdb(conn_str);
}

int check_connection(PGconn* connection)
{
    return PQstatus(connection) != CONNECTION_OK;
}

int check_query_result(PGresult* result, PGconn* connection)
{
    ExecStatusType status = PQresultStatus(result);
    int status_ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    if (!status_ok)
    {
        printf(
            "eps-db: failed to execute query[%s]: %s\n",
            PQresStatus(PQresultStatus(result)),
            PQerrorMessage(connection)
        );
        return 1;
    }
    return 0;
}

int insert_allocation_data(PGconn* connection, eps_allocation_data_t* data)
{
    uint32_t bin_jobid = htobe32((uint32_t) data->jobid);
    uint32_t bin_nnodes = htobe32((uint32_t) data->nnodes);
    uint32_t bin_userid = htobe32((uint32_t) data->userid);

    char ts[12];
    snprintf(ts, 12, "%ld", data->ts);

    int paramFormats[5] = {1, 0, 1, 1, 0};
    const char* paramValues[5] = {
        (char*) &bin_jobid,
        data->jobname,
        (char*) &bin_nnodes,
        (char*) &bin_userid,
        ts
    };
    int paramLengths[5] = {
        sizeof(bin_jobid),
        sizeof(data->jobname),
        sizeof(bin_nnodes),
        sizeof(bin_userid),
        sizeof(ts)
    };
    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO allocations ("ALLOC_COLS") "
        "VALUES($1, $2, $3, $4, to_timestamp($5));",
        5,
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

int
insert_execution_data(PGconn* connection, eps_execution_data_t* data, int* id)
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
        "INSERT INTO executions ("EXEC_COLS")"
        " VALUES($1, $2, $3, to_timestamp($4), to_timestamp($5)) RETURNING id;",
        5,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        0
    );
    int err = check_query_result(res, connection);
    if (err) return err;

    for (int i = 0; i < PQntuples(res); i++)
    {
      for (int j = 0; j < PQnfields(res); j++)
      {
        *id = strtol(PQgetvalue(res, i, j), NULL, 10);
      }
    }
    PQclear(res);
    return 0;
}

int insert_measurement_data(PGconn* connection, eps_measurement_data_t* data)
{
    char exec_id[12];
    snprintf(exec_id, 12, "%d", data->execution_id);

    char e0[20], e1[20];
    char t0[20], t1[20];

    snprintf(e0, 20, "%llu", data->e0);
    snprintf(e1, 20, "%llu", data->e1);
    snprintf(t0, 20, "%llu", data->t0);
    snprintf(t1, 20, "%llu", data->t1);

    const char* paramValues[7] = {
        exec_id,
        data->device_name,
        data->device_uid,
        e0,
        e1,
        t0,
        t1
    };
    int paramLengths[7] = {
        sizeof(exec_id),
        sizeof(data->device_name),
        sizeof(data->device_uid),
        sizeof(e0),
        sizeof(e1),
        sizeof(t0),
        sizeof(t1)
    };
    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO measurements ("MES_COLS") VALUES($1, $2, $3, $4, $5, $6, $7);",
        7,
        NULL,
        paramValues,
        paramLengths,
        NULL,
        0
    );
    int err = check_query_result(res, connection);
    PQclear(res);
    return err;
}
