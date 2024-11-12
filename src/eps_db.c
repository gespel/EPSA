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
