-- EPS database and user
CREATE USER eps WITH PASSWORD 'eps';
CREATE DATABASE eps OWNER eps;

\connect eps

-- Column names must match ALLOC_COLS / EXEC_COLS / MES_COLS in eps_db.c
CREATE TABLE allocations (
    jobid       INTEGER PRIMARY KEY,
    job_name    TEXT,
    nnodes      INTEGER,
    userid      INTEGER,
    ts          TIMESTAMP
);

CREATE TABLE executions (
    id          SERIAL PRIMARY KEY,
    jobid       INTEGER REFERENCES allocations(jobid),
    node_name   TEXT,
    node_id     INTEGER,
    ts_start    TIMESTAMP,
    ts_end      TIMESTAMP
);

CREATE TABLE measurements (
    id              SERIAL PRIMARY KEY,
    exec_id         INTEGER REFERENCES executions(id),
    device_name     TEXT,
    device_uid      TEXT,
    device_type     TEXT,
    e0              BIGINT,
    e1              BIGINT,
    t0              BIGINT,
    t1              BIGINT,
    utilization     DOUBLE PRECISION
);

GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO eps;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO eps;
