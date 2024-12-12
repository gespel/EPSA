#ifndef _EPS_DB_H
#define _EPS_DB_H

#include <libpq-fe.h>

#include <eps_data.h>

// TODO: Discuss and agree on the approach to make this value(s)
//       configurable.
#define DB_CONN_INFO "postgresql://test:testtest2@10.10.10.142/test"

PGconn* connect_db();

int check_connection(PGconn* connection);

int create_insert_query(
    char* query,
    PGconn* connection,
    const char* table,
    const char** columns,
    int num_cols,
    char* values
);

int insert_allocation_data(PGconn* connection, eps_allocation_data_t* data);

#endif
