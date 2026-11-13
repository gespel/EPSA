/* Integration test for the real src/eps_db.c, in an EMPTY disposable database.
 * See README.md in this directory. Never run against a deployment database.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eps_db.h"

static void sql(PGconn *conn, const char *statement)
{
    PGresult *result = PQexec(conn, statement);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        fprintf(stderr, "%s\n", PQerrorMessage(conn));
        exit(1);
    }
    PQclear(result);
}

static void ready(PGconn *conn, uint32_t jobid, int expected)
{
    int value = -1;
    assert(has_complete_job_measurements(conn, jobid, &value) == 0);
    assert(value == expected);
}

int main(void)
{
    const char *dsn = getenv("EPSA_TEST_DSN");
    assert(dsn && "Set EPSA_TEST_DSN to an EMPTY disposable database");
    PGconn *reader = PQconnectdb(dsn), *writer = PQconnectdb(dsn);
    assert(PQstatus(reader) == CONNECTION_OK);
    assert(PQstatus(writer) == CONNECTION_OK);
    /* A private schema prevents replacing any existing tables. No IF EXISTS
     * or CASCADE: pre-existing schema names cause an immediate failure. */
    sql(reader, "CREATE SCHEMA epsa_regression");
    sql(reader, "SET search_path=epsa_regression");
    sql(writer, "SET search_path=epsa_regression");
    sql(reader, "CREATE TABLE allocations(jobid integer PRIMARY KEY,nnodes integer)");
    sql(reader, "CREATE TABLE executions(id integer PRIMARY KEY,jobid integer,node_name text)");
    /* Numeric counters also exercise the C API's full unsigned value range;
     * upstream BIGINT storage remains limited to signed 64-bit values. */
    sql(reader, "CREATE TABLE measurements(exec_id integer,userid integer,device_name text,"
        "device_uid text,device_type text,e0 numeric,e1 numeric,t0 numeric,t1 numeric,utilization numeric)");

    ready(reader, 42, 0); /* no allocation */
    sql(reader, "INSERT INTO allocations VALUES(42,2),(43,0),(44,-1),(45,NULL),(46,1)");
    ready(reader, 42, 0);
    ready(reader, 43, 0);
    ready(reader, 44, 0);
    ready(reader, 45, 0);
    sql(reader, "INSERT INTO executions VALUES(4,46,NULL),(5,46,'node-c')");
    sql(reader, "INSERT INTO measurements(exec_id) VALUES(4)");
    ready(reader, 46, 0); /* a NULL node name must not satisfy readiness */
    sql(reader, "INSERT INTO measurements(exec_id) VALUES(5)");
    ready(reader, 46, 1); /* single-node job */
    sql(reader, "INSERT INTO executions VALUES(1,42,'node-a'),(2,42,'node-b'),(3,42,'node-a')");
    ready(reader, 42, 0); /* execution rows without measurements */
    sql(reader, "INSERT INTO measurements(exec_id) VALUES(1),(1),(3)");
    ready(reader, 42, 0); /* duplicate devices/executions still one node */
    uint64_t count = 0;
    assert(get_measurement_count(reader, 42, &count) == 0 && count == 3);
    sql(writer, "BEGIN");
    sql(writer, "INSERT INTO measurements(exec_id) VALUES(2)");
    ready(reader, 42, 0); /* uncommitted second node must not count */
    sql(writer, "COMMIT");
    ready(reader, 42, 1);
    assert(get_measurement_count(reader, 42, &count) == 0 && count == 4);
    sql(reader, "INSERT INTO executions VALUES(6,42,'node-extra')");
    sql(reader, "INSERT INTO measurements(exec_id) VALUES(6)");
    ready(reader, 42, 0); /* inconsistent excess nodes must not count as ready */
    sql(reader, "DELETE FROM measurements WHERE exec_id=6");
    sql(reader, "DELETE FROM executions WHERE id=6");
    ready(reader, 42, 1);

    eps_measurement_data_t data = {
        .execution_id=1, .userid=1, .device_name="example.package",
        .device_uid="example", .device_type="cpu", .e0=ULLONG_MAX,
        .e1=ULLONG_MAX, .t0=ULLONG_MAX, .t1=ULLONG_MAX, .utilization=100.25
    };
    assert(insert_measurement_data(reader, &data) == 0);
    PGresult *res = PQexec(reader,
        "SELECT e0::text,e1::text,t0::text,t1::text,utilization::text FROM measurements WHERE userid=1");
    assert(PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1);
    for (int i=0; i<4; i++) assert(strcmp(PQgetvalue(res,0,i),"18446744073709551615") == 0);
    assert(strcmp(PQgetvalue(res,0,4),"100.25") == 0);
    PQclear(res);

    sql(reader, "BEGIN");
    sql(reader, "ALTER TABLE measurements RENAME TO hidden_measurements");
    int value = 99;
    assert(has_complete_job_measurements(reader, 42, &value) != 0 && value == 0);
    sql(reader, "ROLLBACK");
    ready(reader, 42, 1);
    sql(reader, "DROP TABLE measurements,executions,allocations");
    sql(reader, "DROP SCHEMA epsa_regression");
    PQfinish(writer);
    PQfinish(reader);
    puts("PASS: completeness, transaction visibility, count compatibility and formatting");
    return 0;
}
