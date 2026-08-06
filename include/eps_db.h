#ifndef _EPS_DB_H
#define _EPS_DB_H

#include <libpq-fe.h>

#include <eps_data.h>

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

int insert_user_data(PGconn* connection, int userid, const char* username);
int insert_allocation_data(PGconn* connection, eps_allocation_data_t* data);
int insert_execution_data(PGconn* connection, eps_execution_data_t* data, int* id);
int insert_measurement_data(PGconn* connection, eps_measurement_data_t* data);
int get_total_energy_consumption(PGconn* connection, uint32_t jobid, uint64_t* total_energy);
int get_measurement_count(PGconn* connection, uint32_t jobid, uint64_t* count);

#endif
