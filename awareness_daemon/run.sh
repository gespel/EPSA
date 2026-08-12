#!/usr/bin/env bash
#
# Startet den awareness_daemon mit der EPS_DB_CONN_STR des laufenden
# slurmctld (muss auf demselben Host laufen, siehe eps_db_conn_str() im
# test/-Modul für dieselbe Technik). Vermeidet, das DB-Passwort hier im
# Repo abzulegen.
#
# Override: EPS_DB_CONN_STR schon in der Umgebung gesetzt? Wird übernommen.

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -z "${EPS_DB_CONN_STR:-}" ]]; then
    PID="$(pgrep -x slurmctld | head -1)"
    if [[ -z "$PID" ]]; then
        echo "EPS_DB_CONN_STR ist nicht gesetzt und es läuft kein slurmctld hier, aus dem ich es übernehmen könnte." >&2
        echo "Entweder EPS_DB_CONN_STR selbst exportieren, oder das Skript auf dem Host mit laufendem slurmctld starten." >&2
        exit 1
    fi
    CONN="$(sudo -n cat "/proc/$PID/environ" 2>/dev/null | tr '\0' '\n' | grep '^EPS_DB_CONN_STR=' | cut -d= -f2-)"
    if [[ -z "$CONN" ]]; then
        echo "Konnte EPS_DB_CONN_STR nicht aus slurmctld (PID $PID) lesen (sudo nötig?)." >&2
        exit 1
    fi
    export EPS_DB_CONN_STR="$CONN"
    echo "EPS_DB_CONN_STR aus laufendem slurmctld (PID $PID) übernommen."
fi

cd "$REPO_DIR"
exec uv run src/awareness_daemon/main.py
