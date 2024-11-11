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
