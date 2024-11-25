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
#include <eps_utils.h>

#include <limits.h>

#define CHECK_PQ_ERR(res, ret) \
    if (PQresultStatus(res) == PGRES_FATAL_ERROR || PQresultStatus(res) != PGRES_COMMAND_OK)\
        return ret;

#define META_COLS "job_id, t_start, nnodes, user_id"
#define DEVICE_COLS "job_id, nodename, energy, t_start, t_duration, device, exclusiv"

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

int check_query_result(PGresult* result, PGconn* connection)
{
    ExecStatusType status = PQresultStatus(result);
    int status_ok = status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
    if (!status_ok) {
        ERROR(
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

    uint32_t bin_jobid = htonl((uint32_t) jobid);
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

    INFO("Found %d entries in table %s with jobid=%d.", nrows, table, jobid);

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
    uint32_t bin_jobid = htonl((uint32_t) data->jobid);
    uint64_t bin_tstart = htole64((uint64_t) data->tstart);
    uint32_t bin_nnodes = htonl((uint32_t) data->nnodes);
    uint32_t bin_userid = htonl((uint32_t) data->userid);

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

    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO meta ("META_COLS") VALUES($1, $2, $3, $4);",
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

int select_meta_data_by_jobid(
    eps_meta_data_t* data, PGconn* connection, int jobid
){
    PGresult* res = NULL;

    if(row_by_userid(connection, &res, "meta", jobid))
    {
        INFO("No entry found in table meta with jobid=%d!", jobid);
        return 1;
    }
    data->jobid = ntohl(
        *((uint32_t*) PQgetvalue(res, 0, PQfnumber(res, "job_id")))
    );
    data->userid = ntohl(
        *((uint32_t*) PQgetvalue(res, 0, PQfnumber(res, "user_id")))
    );
    data->nnodes = ntohl(
        *((uint32_t*) PQgetvalue(res, 0, PQfnumber(res, "nnodes")))
    );
    data->tstart = le64toh(
        *((uint64_t*) PQgetvalue(res, 0, PQfnumber(res, "t_start")))
    );
    data->resources = NULL;

    PQclear(res);

    return 0;
}

/********************************** DEVICES **********************************/
/* IN connection, data, returns int */
int _insert_device_data(PGconn* connection, eps_device_data_t* data)
{
    uint32_t bin_jobid = htonl((uint32_t) data->jobid);
    uint64_t bin_energy = htole64((uint64_t) data->energy);
    uint64_t bin_tstart = htole64((uint64_t) data->tstart);
    uint64_t bin_duration = htole64((uint64_t) data->duration);
    bool bin_exclusive = (bool) htonl(data->exclusive);

    int paramFormats[7] = {1, 0, 1, 1, 1, 0, 1};
    const char* paramValues[7] = {
        (char*) &bin_jobid,
        data->nodename,
        (char*) &bin_energy,
        (char*) &bin_tstart,
        (char*) &bin_duration,
        data->device,
        (char*) &bin_exclusive
    };
    int paramLengths[7] = {
        sizeof(bin_jobid),
        strlen(data->nodename),
        sizeof(bin_energy),
        sizeof(bin_tstart),
        sizeof(bin_duration),
        strlen(data->device),
        sizeof(bin_exclusive)
    };

    PGresult* res = PQexecParams(
        connection,
        "INSERT INTO devices ("DEVICE_COLS") VALUES($1, $2, $3, $4, $5, $6, $7);",
        7,
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

/* Using transaction so insert multiple data in one commit */
int insert_device_data_bulk_ta(
    PGconn* connection, eps_device_data_t* data, int num_data
){
    PGresult* res;

    res = PQexec(connection, "BEGIN");
    CHECK_PQ_ERR(res, 1);

    for(int i=0; i < num_data; i++)
        _insert_device_data(connection, &data[i]);

    res = PQexec(connection, "COMMIT");
    CHECK_PQ_ERR(res, 1);

    res = PQexec(connection, "END");
    CHECK_PQ_ERR(res, 1);

    PQclear(res);

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
        INFO("No entry found in table devices with jobid=%d!", jobid);
        *err = 1;
        return NULL;
    }
    int nrows = PQntuples(res);
    data = calloc(nrows, sizeof(eps_device_data_t));

    for(int i=0; i<nrows; i++)
    {
        data[i].jobid = ntohl(
            *((uint32_t*) PQgetvalue(res, i, PQfnumber(res, "job_id")))
        );
        data[i].nodename = PQgetvalue(res, i, PQfnumber(res, "nodename"));
        data[i].device = PQgetvalue(res, i, PQfnumber(res, "device"));
        data[i].energy = le64toh(
            *((uint64_t*) PQgetvalue(res, i, PQfnumber(res, "energy")))
        );
        data[i].tstart = le64toh(
            *((uint64_t*) PQgetvalue(res, i, PQfnumber(res, "t_start")))
        );
        data[i].duration = le64toh(
            *((uint64_t*) PQgetvalue(res, i, PQfnumber(res, "t_duration")))
        );
        data[i].exclusive = ntohl(
           *((bool*) PQgetvalue(res, i, PQfnumber(res, "exclusiv")))
        );
        // TODO: finalize
        data[i].resource = NULL;
    }

    PQclear(res);

    *num_elems = nrows;
    *err = 0;

    return data;
}

/*********************************** JOB *************************************/
/* Returns job data or NULL if an error has occurred*/
eps_job_data_t* compose_job_data(PGconn* connection, int jobid)
{
    eps_job_data_t* data = calloc(1, sizeof(eps_job_data_t));
    int num_elems;
    long tstart = LONG_MAX;
    unsigned long long duration, end = 0;
    int err = 0;
    eps_device_data_t* devices = select_device_data_by_jobid(
        &num_elems, &err, connection, jobid
    );
    if(err)
     return NULL;

    for(int i=0; i<num_elems; i++)
    {
        if(devices[i].jobid != jobid)
            continue;
        data->jobid = devices[i].jobid;
        data->energy += devices[i].energy;
        if(devices[i].tstart < tstart)
        {
            data->tstart = devices[i].tstart;
        }
        duration = devices[i].duration;
        unsigned long long tmp_end = tstart + duration;
        if(tmp_end > end)
            end = tmp_end;
        /* use duration from ealierst start to latest end */
        data->runtime = data->tstart - end;
        /* use longest measurement duration */
        if(duration > data->duration)
            data->duration = duration;
    }
    return data;
}

int has_valid_db_entries(PGconn* connection, int jobid)
{
    INFO("DB validation for jobid=%d...", jobid);
    int num_devices, err = 0;
    eps_meta_data_t meta;

    err = select_meta_data_by_jobid(&meta, connection, jobid);
    if(err)
        return err;

    select_device_data_by_jobid(
        &num_devices, &err, connection, jobid
    );

    if(err)
        return err;

    if(num_devices == 0)
    {
        INFO("No device entries found.");
        return 1;
    }

    uint32_t bin_jobid = htonl((uint32_t) jobid);
    int paramFormats[1] = {1};
    const char* paramValues[1] = {(char*) &bin_jobid};
    int paramLengths[1] = {sizeof(bin_jobid)};

    PGresult* res = PQexecParams(
        connection,
        "SELECT DISTINCT nodename FROM devices WHERE job_id = $1;",
        1,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        0
    );

    int used_nodes = PQntuples(res);
    PQclear(res);

    int ret = meta.nnodes == used_nodes;
    if(ret)
        INFO("DB validation for jobid=%d was successful!", jobid);
    else
        INFO("DB validation for jobid=%d FAILED!", jobid);

    return ret;
}
