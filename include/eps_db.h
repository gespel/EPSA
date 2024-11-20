#ifndef _EPS_DB_H
#define _EPS_DB_H

#include <eps_data.h>
#include <libpq-fe.h>

// TODO: Discuss and agree on the approach to make this value(s)
//       configurable.
#define DB_CONN_INFO "postgresql://test:testtest2@10.10.10.142/test"

#define MAX_QUERY_SIZE 1024
#define VALUES_BUFFER_SIZE 256

#define META_COLS_SIZE 4
#define META_DATA_VALS(db_conn, data, str) \
    do{\
        sprintf(\
            str,\
            "%d,%ld,%d,%d",\
            data->jobid,\
            data->tstart,\
            data->nnodes,\
            data->userid\
        );\
    } while(0)

#define DEVICE_COLS_SIZE 6
#define DEVICE_DATA_VALS(db_conn, data, str) \
    do{\
        sprintf(\
            str,\
            "%d,%s,%s,%llu,%ld,%lld",\
            data->jobid,\
            PQescapeLiteral(db_conn, data->nodename, strlen(data->nodename)),\
            PQescapeLiteral(db_conn, data->device, strlen(data->device)),\
            data->energy,\
            data->tstart,\
            data->duration\
        );\
    } while(0)

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

int insert_meta_data(PGconn* connection, eps_meta_data_t* data);
int insert_device_data(PGconn* connection, eps_device_data_t* data);
int insert_device_data_bulk_ta(
    PGconn* connection, eps_device_data_t* data, int num_data
);

eps_device_data_t* select_device_data_by_jobid(
    int* num_elems, int* err, PGconn* db_conn, int jobid
);
int select_meta_data_by_jobid(eps_meta_data_t* data, PGconn* db_conn, int jobid);
#endif
