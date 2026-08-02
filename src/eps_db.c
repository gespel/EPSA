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
#define MES_COLS "exec_id, device_name, device_uid, device_type, " \
                 "e0, e1, t0, t1, utilization"

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

    int param_formats[5] = {1, 0, 1, 1, 0};
    const char* param_values[5] = {
        (char*) &bin_jobid,
        data->jobname,
        (char*) &bin_nnodes,
        (char*) &bin_userid,
        ts
    };
    int param_lengths[5] = {
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
        param_values,
        param_lengths,
        param_formats,
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

    int param_formats[5] = {1, 0, 1, 0, 0};
    const char* param_values[5] = {
        (char*) &bin_jobid,
        data->nodename,
        (char*) &bin_nodeid,
        tstart,
        tend
    };
    int param_lengths[5] = {
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
        param_values,
        param_lengths,
        param_formats,
        0
    );
    int err = check_query_result(res, connection);
    if (err) {
        PQclear(res);
        return err;
    }

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
    char utilization[6];

    snprintf(e0, 20, "%llu", data->e0);
    snprintf(e1, 20, "%llu", data->e1);
    snprintf(t0, 20, "%llu", data->t0);
    snprintf(t1, 20, "%llu", data->t1);
    snprintf(t1, 20, "%llu", data->t1);
    snprintf(utilization, 6, "%.2f", data->utilization);

    const char* param_values[9] = {
        exec_id,
        data->device_name,
        data->device_uid,
        data->device_type,
        e0,
        e1,
        t0,
        t1,
        utilization
    };
    int param_lengths[9] = {
        sizeof(exec_id),
        sizeof(data->device_name),
        sizeof(data->device_uid),
        sizeof(data->device_type),
        sizeof(e0),
        sizeof(e1),
        sizeof(t0),
        sizeof(t1),
        sizeof(utilization)
    };
    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO measurements ("MES_COLS") VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9);",
        9,
        NULL,
        param_values,
        param_lengths,
        NULL,
        0
    );
    int err = check_query_result(res, connection);
    PQclear(res);
    return err;
}

int get_total_energy_consumption(PGconn* connection, uint32_t jobid, uint64_t* total_energy)
{
    char jobid_str[12];
    snprintf(jobid_str, 12, "%d", jobid);

    const char* param_values[1] = {jobid_str};
    int param_lengths[1] = {sizeof(jobid_str)};

    PGresult* res = PQexecParams(
        connection,
        "SELECT SUM(e1 - e0) AS total_energy "
        "FROM measurements "
        "JOIN executions ON measurements.exec_id = executions.id "
        "WHERE executions.jobid = $1;",
        1,
        NULL,
        param_values,
        param_lengths,
        NULL,
        0
    );
    int err = check_query_result(res, connection);
    if (err) {
        PQclear(res);
        return err;
    }

    if (PQntuples(res) > 0 && PQgetisnull(res, 0, 0) == 0) {
        *total_energy = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
    } else {
        *total_energy = 0;
    }

    PQclear(res);
    return 0;
}

int get_measurement_count(PGconn* connection, uint32_t jobid, uint64_t* count)
{
    char jobid_str[12];
    snprintf(jobid_str, 12, "%d", jobid);

    const char* param_values[1] = {jobid_str};
    int param_lengths[1] = {sizeof(jobid_str)};

    PGresult* res = PQexecParams(
        connection,
        "SELECT COUNT(*) "
        "FROM measurements "
        "JOIN executions ON measurements.exec_id = executions.id "
        "WHERE executions.jobid = $1;",
        1,
        NULL,
        param_values,
        param_lengths,
        NULL,
        0
    );
    int err = check_query_result(res, connection);
    if (err) {
        PQclear(res);
        return err;
    }

    if (PQntuples(res) > 0 && PQgetisnull(res, 0, 0) == 0) {
        *count = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
    } else {
        *count = 0;
    }

    PQclear(res);
    return 0;
}