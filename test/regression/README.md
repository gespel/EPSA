# Focused regression tests

Run from the repository root. These tests do not install a plugin, restart
Slurm, start an HTTP listener or send email.

## Awareness daemon

With the project's Python dependencies available:

```sh
PYTHONPATH=awareness_daemon/src python3 -m unittest discover \
  -s test/regression -p 'test_*.py' -v
```

Database connections and SMTP interactions are mocked. Coverage includes:

- disabled/missing/non-boolean email opt-in performs no recipient query,
  message preparation or send;
- enabled email continues to prepare and send a batch;
- weekly default and explicit interval override;
- connection errors retain their original exception instead of being masked by
  an uninitialized local variable;
- opened connections close on normal and exceptional exits;
- an unknown user returns `None` without logging a spurious database error;
- package import does not initialize the Flask application or discover secrets;
- the lazy command entrypoint remains callable after its first invocation.

## C database integration

Requires a C compiler, libpq development files, the configured Slurm source
headers used to build EPSA, and PostgreSQL. **Use a new disposable database,
never the deployment database.** The test creates and drops its own schema,
`epsa_regression`, without `IF EXISTS`/`CASCADE`. A schema-name conflict fails
instead of replacing anything. Failed assertions can leave this test schema
behind; discard the disposable database after inspecting the failure.

For a local PostgreSQL role allowed to create databases:

```sh
createdb epsa_regression_test
test_build=$(mktemp -d)
cc -std=gnu11 -O0 -g -Iinclude -I"$(pg_config --includedir)" \
  -I"$SLURM_SRC_DIR" test/regression/test_db.c src/eps_db.c \
  -L"$(pg_config --libdir)" -lpq -o "$test_build/test_db"
EPSA_TEST_DSN='dbname=epsa_regression_test' "$test_build/test_db"
dropdb epsa_regression_test
```

Do not compile this assertion-based test with `-DNDEBUG`. The deliberate
missing-table case prints one PostgreSQL error, then should end with `PASS`.
The test checks:

- absent, NULL, zero-node and negative-node allocations are not complete;
- NULL node names do not count, single-node jobs pass, and excess distinct
  nodes fail closed;
- execution rows without measurements are not complete;
- multiple device rows/duplicate executions from one node cannot substitute
  for another allocated node;
- a second connection's uncommitted measurements remain invisible;
- completeness becomes true after the missing node commits;
- `get_measurement_count()` retains its original row-count contract;
- readiness is cleared and an error returned when a query fails;
- full unsigned counters and the decimal formatting boundary are not truncated.

The counter fixture uses `numeric` columns to exercise the full unsigned C
range. The upstream schema's `BIGINT` still supports only signed 64-bit values;
this patch does not change that schema or claim otherwise.

## Deployment considerations

The controller retains upstream's 15-second accumulated polling-sleep budget
and 100-ms poll interval. Database connection/query time is not included, so
this is NOT a 15-second wall-clock deadline. If data is incomplete at timeout
it logs a warning and skips
the summary, without failing the completed job solely for missing telemetry.
Committed rows remain available for inspection. Query failures retain the
existing error path.

Compared with accepting the first available row, normal multi-node jobs can
keep the asynchronous controller epilog pending longer. Missing/unsupported
telemetry on any node suppresses the central log summary; there is no later
automatic backfill. The check counts distinct node names, not the allocated
host list, and does not deduplicate energy sums or validate device coverage.
The query is more expensive than a row count and runs at up to ten polls per
second per finishing job. Large-history/high-concurrency deployments need
query-plan/index review and explicit connection/statement timeouts; this PR
does not add indexes, connection pooling or bounded database I/O.

This depends on the EFP committing an execution and all its measurement rows
in one transaction. It does not add an explicit completion marker or distinguish
multiple attempts of a requeued/resized job. It also does not fix overlapping
energy-domain accounting or shared-node attribution.

The integration test exercises database readiness, not the real-time Slurm
epilog callback/timeout lifecycle. A controlled cluster smoke test remains
appropriate before deployment. The Slurm 25.11 build has been checked; older
Slurm API branches and GPU hardware require separate validation.

Email opt-in now requires a boolean `true`; strings such as `"true"` and numeric
`1` no longer enable it. The seven-day interval is only a fallback: upstream's
example config explicitly sets `sending_interval: 60` and remains unchanged.
The first enabled report still runs immediately on daemon start, not seven
days later. This PR does not persist a last-sent timestamp across restarts or
validate malformed/null email config sections.

The awareness HTTP application remains without built-in user authentication.
Keep it behind an authenticated access boundary; do not expose all users' job
data publicly. This patch contains no site credentials or authentication setup.
