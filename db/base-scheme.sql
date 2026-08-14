CREATE TABLE users (
    userid      INTEGER PRIMARY KEY,
    username    TEXT,
    email       TEXT
);

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
    userid      INTEGER REFERENCES users(userid),
    ts_start    TIMESTAMP,
    ts_end      TIMESTAMP
);

CREATE TABLE measurements (
    id              SERIAL PRIMARY KEY,
    exec_id         INTEGER REFERENCES executions(id),
    userid          INTEGER REFERENCES users(userid),
    device_name     TEXT,
    device_uid      TEXT,
    device_type     TEXT,
    e0              BIGINT,
    e1              BIGINT,
    t0              BIGINT,
    t1              BIGINT,
    utilization     DOUBLE PRECISION
);