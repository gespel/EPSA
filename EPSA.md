# EPSA – Build, Installation & lokales Testen

Praxis-Notizen zum Bauen, Installieren und lokalen Testen des `prep_eps`
Plugins. Ergänzt das [README.md](./README.md) (dort stehen die
Voraussetzungen/Prerequisites im Detail) um alles, was für einen konkreten
lokalen Single-Node-Testlauf nötig ist, inklusive einiger Stolperfallen, die
beim Testen aufgefallen sind.

## Table Of Contents

1. [Bauen](#bauen)
2. [Installieren](#installieren)
3. [Lokalen Testcluster konfigurieren](#lokalen-testcluster-konfigurieren)
4. [Bekannte Stolperfallen](#bekannte-stolperfallen)
5. [Wichtige Slurm-Befehle](#wichtige-slurm-befehle)
6. [Ergebnisse in der DB prüfen](#ergebnisse-in-der-db-prüfen)

## Bauen

```bash
mkdir -p build && cd build
cmake .. \
  -DSLURM_INSTALL_DIR=/usr \
  -DSLURM_SRC_DIR=/pfad/zu/slurm-quellcode \
  -DEMA_INSTALL_DIR=/pfad/zu/EMA/install \
  -DPQ_INSTALL_DIR=/usr \
  -DHWLOC_INSTALL_DIR=/usr
make prep_eps
```

Ergebnis: `build/prep_eps.so`.

- `SLURM_SRC_DIR` muss exakt (bis auf die Minor-Version) zur installierten
  Slurm-Version passen (siehe `slurmd -V`).
- Nach Änderungen an `src/*.c` reicht `make prep_eps` im `build`-Verzeichnis
  für einen inkrementellen Rebuild.

## Installieren

```bash
sudo cp build/prep_eps.so /usr/lib/slurm/prep_eps.so
sudo chmod 755 /usr/lib/slurm/prep_eps.so
```

`/usr/lib/slurm` ist der Standard-`PluginDir`, den `slurmd`/`slurmctld` beim
Arch-Paket `slurm-llnl` durchsuchen (bzw. der in `slurm.conf` unter
`PluginDir=` konfigurierte Pfad).

Nach jedem Neubau: Datei erneut kopieren und `slurmctld`+`slurmd` neu starten
(Plugins werden nur beim Daemon-Start geladen):

```bash
sudo systemctl restart slurmctld slurmd
```

## Lokalen Testcluster konfigurieren

Minimal-Setup für Controller + Compute-Node auf demselben Host:

- **Munge**: `munge.key` vorhanden, `munge.service` läuft.
- **PostgreSQL**: Datenbank `eps` mit den Tabellen `allocations`,
  `executions`, `measurements` (Spalten siehe `src/eps_db.c` /
  `include/eps_data.h`).
- **`slurm.conf`** (Kernpunkte, Rest wie `slurm.conf.example`):
  ```
  PrEpPlugins=prep/eps
  PrologFlags=Alloc,Serial
  Epilog=/bin/true
  ```
- **`EPS_DB_CONN_STR`** muss für **beide** Daemons gesetzt sein (siehe
  [Bekannte Stolperfallen](#bekannte-stolperfallen)):
  - `/etc/default/slurmd`
  - `/etc/default/slurmctld`
  ```
  EPS_DB_CONN_STR=postgresql://eps:<passwort>@localhost/eps
  ```

## Bekannte Stolperfallen

Diese Punkte kosten beim ersten lokalen Testlauf am meisten Zeit:

1. **`Epilog=` muss gesetzt sein, sonst läuft der PrEp-Epilog nie.**
   `slurmd` hat in `_rpc_terminate_job()`
   (`src/slurmd/slurmd/req.c`) einen Bypass: sind bei Eintreffen der
   Terminate-RPC keine aktiven Steps mehr da (`nsteps == 0` – das ist bei
   jedem normal beendeten Job der Fall, unabhängig von der Laufzeit) **und**
   kein `Epilog=`-Skript konfiguriert, wird `run_epilog()` – und damit
   `prep_p_epilog` – komplett übersprungen. Ohne ein (Dummy-)`Epilog=`-Skript
   (z. B. `/bin/true`) bleiben `executions`/`measurements` immer leer, egal
   wie der Job sich verhält.

2. **`EPS_DB_CONN_STR` wird sowohl von `slurmd` als auch von `slurmctld`
   gebraucht.** `prep_p_prolog_slurmctld`/`prep_p_epilog_slurmctld` laufen im
   `slurmctld`-Prozess, `prep_p_prolog`/`prep_p_epilog` im `slurmd`-Prozess –
   beide öffnen eigenständig eine DB-Verbindung.

3. **`libEMA.so` muss für den `slurm`-Systemuser erreichbar sein.** Liegt die
   EMA-Installation unter einem Home-Verzeichnis mit restriktiven Rechten
   (z. B. `700`), kann `slurmctld` (läuft als User `slurm`) das Plugin nicht
   `dlopen`en (`libEMA.so: cannot open shared object file`), obwohl `ldd`
   als Dev-User fehlerfrei aussieht. Fix: Lib zusätzlich systemweit
   verfügbar machen, z. B.:
   ```bash
   sudo cp /pfad/zu/EMA/install/lib/libEMA.so /usr/local/lib/
   echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/eps-ema.conf
   sudo ldconfig
   ```

4. **Verwaiste EFP-Kindprozesse blockieren den `slurmd`-Neustart.** Der
   EFP-Messprozess wird im Prolog geforkt und bleibt bis zum Signal aus dem
   Epilog am Leben. Läuft der Epilog aus irgendeinem Grund nicht (siehe
   Punkt 1, oder ein Crash mittendrin), bleibt der EFP-Prozess für immer als
   `EFP waiting...` hängen (Log: `/var/log/eps/efp_<jobid>.log`) – und hält
   dabei den von `slurmd` geerbten Listen-Socket offen, was einen Neustart
   mit `Address already in use` blockiert. Betroffene PIDs identifizieren
   (`ps aux | grep "slurmd --systemd"`, alle außer dem aktuellen Hauptprozess)
   und mit `kill -9` beenden, bevor `slurmd` neu gestartet wird.

5. **Node ohne konfiguriertes GRES**: In Slurm-Versionen mit dem gefixten
   `parse_gres`/`_process_gres_count` (siehe `src/eps_utils.c`,
   `src/eps_gres.c`) ist das kein Problem mehr – vor dem Fix führte ein Node
   ganz ohne `Gres=` in `slurm.conf` zu einem `slurmd`-Crash (`strdup(NULL)`)
   bzw. danach zu einem fehlgeschlagenen Prolog inkl. `DRAIN` des Nodes.

6. **Jobs bleiben mit `launch failed requeued held` hängen** (Log:
   `cannot setup the scope for cgroup`): Der systemd-Scope
   `slurmstepd.scope` ist tot (`journalctl -u slurmstepd.scope` zeigt
   `Deactivated`), meist weil sein Keep-Alive-Prozess bei einem früheren
   `slurmd`-Neustart mitbeendet wurde. Fix: `sudo systemctl restart
   slurmctld slurmd` – dabei legt systemd den Scope neu an.

## Wichtige Slurm-Befehle

### Dienste

```bash
sudo systemctl restart munge postgresql slurmctld slurmd
sudo systemctl status slurmctld slurmd --no-pager
journalctl -u slurmctld -u slurmd -f          # live mitlesen
sudo tail -f /var/log/slurm-llnl/slurmd.log
sudo tail -f /var/log/slurm-llnl/slurmctld.log
```

### Cluster-/Node-Status

```bash
sinfo                          # Partitions- und Node-Übersicht
scontrol show node <name>      # Detailstatus eines Nodes
scontrol update NodeName=<name> State=RESUME   # Node aus DRAIN/DOWN holen
```

### Testjobs starten

```bash
# Interaktiv, blockierend, gut für schnelle Checks
srun -N1 -n1 sleep 5

# Batch-Job (näher an echtem Produktionsverhalten)
cat > test.sbatch <<'EOF'
#!/bin/bash
#SBATCH -N1 -n1
sleep 30
EOF
sbatch test.sbatch
```

### Jobs beobachten

```bash
squeue                         # Warteschlange / laufende Jobs
scontrol show job <jobid>      # Detailstatus eines Jobs
scancel <jobid>                # Job abbrechen (z. B. wenn im Prolog hängt)
```

### Debug-Level erhöhen (bei Bedarf)

```bash
sudo scontrol setdebug debug3
sudo scontrol setdebugflags +Agent +Protocol
```
Für dauerhaft mehr Detail in `slurmd`s Log: `SlurmdDebug=debug` in
`slurm.conf` setzen und `slurmd` neu starten (die o.g. `debug()`-Level-Zeilen
in `req.c` erscheinen sonst gar nicht im Log).

## Ergebnisse in der DB prüfen

```bash
psql "postgresql://eps:<passwort>@localhost/eps" \
  -c "SELECT * FROM allocations ORDER BY jobid;" \
  -c "SELECT * FROM executions ORDER BY id;" \
  -c "SELECT id, exec_id, device_name, device_type, e1-e0 AS energy_delta, utilization FROM measurements ORDER BY id;"
```

Ein erfolgreicher Testlauf zeigt:
- Einen Eintrag in `allocations` (aus dem `slurmctld`-Prolog, sobald der Job
  alloziert wird).
- Einen Eintrag in `executions` pro Node/Job (aus dem `slurmd`-Epilog).
- Zwei oder mehr Einträge in `measurements` pro `execution` (z. B.
  `CPU-0.package-0` und `CPU-0.core`), mit `e1 > e0` als gemessenem
  Energiedelta.
