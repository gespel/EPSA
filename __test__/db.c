#include <eps_db.h>

PGconn* connection = NULL;

void exit_on_error(int ret, const char* msg)
{
    if(ret)
    {
        printf("%s: %s\n", msg, PQerrorMessage(connection));
        PQfinish(connection);
        exit(1);
    }
}

int main(int argc, char** argv){
    if(argc != 2)
    {
        printf("Add programm argument: %s <jobid>\n", __FILE__);
        return 0;
    }
    int jid = (int) strtol(argv[1], (char **)NULL, 10);

    /* Connecting DB. */
    connection = PQconnectdb(DB_CONN_INFO);
    printf("DB connected.\n");

    /* Checking DB connection. */
    int err = check_connection(connection);
    if (err) {
        exit_on_error(err, "Connection check failed!");
    }

    /* Disconnecting DB. */
    PQfinish(connection);
    printf("DB disconnected.\n");
}
