#ifndef _EPS_DB_H
#define _EPS_DB_H

#include <eps_data.h>
#include <libpq-fe.h>

// TODO: Discuss and agree on the approach to make this value(s)
//       configurable.
#define DB_CONN_INFO "postgresql://test:testtest2@10.10.10.142/test"

#define MAX_QUERY_SIZE 1024
#define VALUES_BUFFER_SIZE 256

#define META_COLS "job_id, t_start, nnodes, user_id"
#define META_DATA_VALS(data, str) \
    do{\
        sprintf(\
            str,\
            "%d,'%s',%d,%d",\
            data->jobid, data->tstart, data->nnodes, data->userid\
        );\
    } while(0)

#define JOB_COLS "job_id, energy, t_start, t_duration"
#define JOB_DATA_VALS(data, str) \
    do{\
        sprintf(\
            str,\
            "%d,%llu,'%s',%llu",\
            data->jobid,\
            (long long int) data->energy,\
            data->tstart,\
            (long long int) data->duration\
        );\
    } while(0)

// #define DEVICE_COLS "job_id, nodename, device, device_type, energy, t_start, t_duration, exclusive"
#define DEVICE_COLS "job_id, nodename, device, energy, t_start, t_duration"
#define DEVICE_DATA_VALS(data, str) \
    do{\
        sprintf(\
            str,\
            "%d,'%s','%s',%llu,'%s',%lld",\
            data->jobid,\
            data->nodename,\
            data->device,\
            (long long int) data->energy,\
            data->tstart,\
            data->duration\
        );\
    } while(0)

PGconn* connect_db();

int check_connection(PGconn* connection);

int insert(
    char* query,
    const char* table,
    const char* columns,
    char* values
);

int insert_meta_data(PGconn* connection, eps_meta_data_t* data);
int insert_job_data(PGconn* connection, eps_job_data_t* data);
int insert_device_data(PGconn* connection, eps_device_data_t* data);

#endif
