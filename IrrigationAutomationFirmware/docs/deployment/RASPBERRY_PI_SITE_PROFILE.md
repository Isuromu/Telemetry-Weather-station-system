# Raspberry Pi site profile

Operational snapshot of the irrigation Raspberry Pi. Update this file when the
host is rebuilt, moved, or significantly reconfigured. All figures below are
observations, not requirements.

## Measurement provenance

| Field | Value |
|---|---|
| Measured | 2026-10-17 12:28 local time |
| Uptime at measurement | 2 h 26 min |
| Method | Read-only shell inspection |
| Host address | `192.168.1.116` |

> Address note: earlier deployment notes name `192.168.2.217`. Re-confirm the
> Pi's current address before pointing the mobile app or scripts at it.

## Hardware and operating system

| Item | Value |
|---|---|
| Model | Raspberry Pi 4 Model B Rev. 1.5 |
| CPU | 4 × Cortex-A72, aarch64 |
| RAM | 8 GB (7.63 GiB usable) |
| OS | Debian 13 "trixie" 13.4 |
| Kernel | 6.12.75+rpt-rpi-v8 (64-bit) |
| Storage | 16 GB microSD (SC16G, dated 03/2022, DDR50 SDHC), single ext4 root |
| GPU memory split | 76 MB |
| Power/thermal | 37.9 °C, `throttled=0x0`, 0.946 V, governor `ondemand` |
| Boot mode | Graphical (`graphical.target`), lightdm/labwc/Chromium kiosk |

## Storage at measurement

| Mount | Size | Used | Free |
|---|---|---|---|
| `/` (ext4, `mmcblk0p2`) | 15.07 GB | 14.07 GB | ~300 MB (98%) |
| `/boot/firmware` | 505 MB | 86 MB | 419 MB (17%) |

Inode use: 34%.

Largest consumers:

| Path | Size |
|---|---|
| `/usr` | 6.1 GB |
| `/home` | 3.7 GB |
| `/home/maxi/.rustup` | 2.5 GB |
| `/home/maxi/chirpstack-concentratord` | 464 MB |
| `/home/maxi/.node-red` | 148 MB |
| `/var/lib/influxdb/engine` | 2.1 GB |
| Docker/containerd | ~36 MB |

## Time synchronization

Verified 2026-10-17:

| Check | Result |
|---|---|
| `System clock synchronized` | yes |
| `NTP service` | active |
| `RTC in local TZ` | no |
| `chronyc` | not installed |

Time source is `systemd-timesyncd`, not chrony. The server should gate on
`timedatectl show -p NTPSynchronized --value` (currently `yes`) and on the NTP
service being active. `systemd-timesyncd` exposes a synchronized yes/no, not a
measured offset; do not claim a precision threshold that it cannot report.
The Pi 4 has no battery-backed RTC by default, so a cold boot before NTP sync
must be treated as unsynchronized.

## Installed versions

| Component | Version |
|---|---|
| Node-RED | 5.0.4 (`systemd nodered.service`, enabled; port 1880 → 200) |
| Node.js / npm | v24.19.0 / 11.17.0 |
| `@flowfuse/node-red-dashboard` | 1.30.2 |
| Node-RED flow file | `~/.node-red/flows.json`, ~314 KB |
| InfluxDB | 2.9.1 (`influxdb2` package, systemd, `/health` pass) |
| Influx CLI | 2.8.0 |
| ChirpStack | 4.18.0 (SX1302 concentratord + MQTT forwarder + REST API) |
| Mosquitto | 2.0.21 |
| Redis | 8.0.2 |
| PostgreSQL | 17 |
| Docker CE | 29.6.0 (running, zero containers) |

## Memory baseline

| Item | Value |
|---|---|
| Used | 1.6 GiB |
| Available | 6.1 GiB |
| Buff/cache | 1.6 GiB |
| Swap | 2 GiB zram, 0 used |
| Load average | 0.20–0.58 |
| CPU idle | 94–99% |

Note: systemd cgroup memory accounting is disabled (`MemoryCurrent` unset).

RSS by process group (RSS sum; may overcount shared pages):

| Process group | RSS |
|---|---|
| Chromium desktop/kiosk (17 processes) | ~1.9 GB |
| ChirpStack stack (4 services) | ~373 MB |
| PostgreSQL | ~366 MB |
| containerd + dockerd | ~215 MB |
| InfluxDB (`influxd`) | ~214 MB |
| Node-RED | ~165 MB |
| labwc + panel + Xwayland | ~165 MB |
| Soil valve controller (Python) | ~47 MB |
| Mosquitto | ~12 MB |
| Redis | ~17 MB |

## Deployment constraints

1. **Root filesystem is 98% full (~300 MB free).** InfluxDB holds 2.1 GB and
   retention cannot be verified without a read token (`/api/v2/buckets` returns
   unauthorized).
2. **24/7 writes on a consumer microSD from 2022.** InfluxDB, Node-RED logs, and
   the desktop all write to it. No SMART telemetry; wear and I/O reliability are
   structural risks. A USB SSD is the durable fix.
3. **The host is not headless.** `graphical.target` with lightdm/labwc/Chromium
   consumes roughly 2 GB RSS.
4. **Easiest disk reclaim:** `/home/maxi/.rustup` (2.5 GB) or InfluxDB retention.
5. **Docker/containerd are enabled and idle** (~215 MB RAM, ~36 MB disk);
   removable if unused.
6. **Node-RED 5.0.4 on Node 24 and InfluxDB 2.9.1** are current, maintained
   releases. InfluxDB uses the TSM engine.

## Implications for the Phase 6 local server

- Disk headroom is the first blocker. A Python virtual environment, logs, and a
  SQLite database need more than 300 MB of free space. Reclaim space before
  installing the server.
- The Pi has enough RAM for a FastAPI service, but Chromium kiosk is the largest
  consumer. Replacing the Node-RED dashboard with a server-served page makes the
  kiosk and desktop stack removable, recovering roughly 2 GB of RAM and its CPU
  share.
- Redis and PostgreSQL are already running. Neither is required for the first
  server version: SQLite plus an in-process cache is simpler. Do not add a
  dependency merely because the daemon exists.
- Docker is idle. If nothing else uses it, disabling it frees memory and removes
  a moving part.
- Node-RED must remain fully running until the server passes all tests,
  including real-device watering. Do not remove dashboard flows or the
  temporary API during Phase 6; add Influx event nodes additively. Any later
  slimming is a separate, explicitly approved task after an observation period.

## InfluxDB usage

- Organization: `amudario`.
- Existing demo write: MainValve flow, bucket `main_valve`, measurement
  `main_valve_data`, field `freq`. Left untouched.
- Writer: Node-RED through `node-red-contrib-influxdb` 0.7.0.
- Phase 6 schema: [`../integrations/watering-plan/INFLUX_SCHEMA.md`](../integrations/watering-plan/INFLUX_SCHEMA.md)
  (buckets `irrigation_telemetry` and `irrigation_events`).

## Phase 0 preflight results (2026-10-17)

Executed over SSH as `maxi@192.168.1.116`, the address re-confirmed in this
session; `192.168.2.217` no longer answers. Node-RED stayed running and no flow
was changed. Measurements taken 15:53-16:03 local time.

### Disk

| Point | Free | Use |
|---|---|---|
| Before | 293 MB | 98% |
| After cleanup | 3.0 GB | 79% |

Reclaimed:

- `/home/maxi/.rustup` removed: 2.5 GB.
- `apt-get clean`: 133 MB.
- User caches removed: `~/.cache/node-gyp` 65 MB, `~/.cache/chromium` 28 MB,
  `~/.cache/pip` 19 MB.
- `docker.service`, `containerd.service` and `docker.socket` disabled and
  stopped (about 215 MB RAM and 36 MB disk). The host actually had one
  **created, never started** `eclipse-mosquitto` container and its 37 MB image,
  which the earlier snapshot missed. Both were left in place.
- `journalctl --vacuum-time=7d` freed 0 B. The 135 MB journal is younger than
  seven days and was deliberately retained as pump/downlink evidence.

InfluxDB still occupies 2.1 GB under `/var/lib/influxdb`. No existing data was
deleted: `main_valve` is protected by the plan, and `_monitoring`/`_tasks` carry
their own retention. The two new buckets have 90-day and 365-day retention from
creation.

### Clock

`NTPSynchronized=yes`; `systemd-timesyncd` is active and enabled. No offset
threshold is claimed, because the service reports only a boolean.

### Backups

`/home/maxi/backups/phase6-20261017/` (mode 700) and
`/home/maxi/backups/phase6-20261017.tar.gz` (mode 600) contain:

- `flows.json` (165 nodes), `flows_cred.json`, `settings.js`,
  `.config.runtime.json`, `environment`, `package.json`;
- `watering-plans.sqlite3`, taken with the SQLite backup API while Node-RED was
  running and verified: `PRAGMA integrity_check` = ok, 33 plans, 73 plan
  events, 0 attempts, 2 schema migrations.

`~/.node-red/environment` and `flows_cred.json` were world-readable (mode 644)
and were tightened to 600.

A pre-migration-003 database copy and the prior module/migration files are in
`/home/maxi/backups/phase6-20261018/` (created before the 2026-10-18 update).

Restore procedure:

1. `sudo systemctl stop nodered`.
2. Copy the Node-RED files, including `.config.runtime.json`, back to
   `~/.node-red/` as `maxi:maxi`, mode 600. `.config.runtime.json` matters:
   without it Node-RED generates a new credential secret and cannot decrypt the
   backed-up `flows_cred.json`.
3. Remove `watering-plans.sqlite3-wal` and `-shm`, copy the backed-up
   `watering-plans.sqlite3` to `/var/lib/node-red/`, and re-run the integrity
   and row-count checks.
4. `sudo systemctl start nodered`; check `NRestarts`,
   `GET /api/v1/health`, and the dashboard.

### Python

- Python 3.13.5 at `/usr/bin/python3`.
- Venv: `/home/maxi/irrigation-server/.venv` (62 MB). Direct dependencies are
  pinned in `server/pyproject.toml`; the full resolved set is
  `server/requirements.lock`.
- Importing the whole stack measured 60.0 MB maximum RSS. The server's
  steady-state RSS is recorded after P1 starts serving.
- Node-RED remained at 163 MB RSS with `NRestarts=0`.
- Memory after cleanup: 1.6 GiB used, 6.0 GiB available.

### InfluxDB

`GET /health` answers 200. The Phase 6 buckets exist in org `amudario`:

| Bucket | ID | Retention | Shard group |
|---|---|---|---|
| `irrigation_telemetry` | `28b3e389bd453fa1` | 90 days (7,776,000 s) | 1 day |
| `irrigation_events` | `f9e0cc9ca20e3716` | 365 days (31,536,000 s) | 7 days |

`main_valve` was not touched. Two bucket-scoped tokens were created and
verified by behaviour; no token value is recorded here or in git:

- **Node-RED write:** write to `irrigation_telemetry` and `irrigation_events`;
  write to `main_valve` is denied (403); no query access.
- **Server read:** query access to both new buckets; write is denied (403);
  cannot list buckets or authorizations.

The two org-wide tokens supplied for this step were deactivated after the
scoped replacements passed verification. Their values appeared in a chat
session, so treat them as compromised even though they are inactive.

The scoped credentials live only on the Pi, mode 600:
`/home/maxi/irrigation-server/.env` (server read plus bucket/org settings) and
`/home/maxi/irrigation-server/.node-red-write.token` (staging for the Node-RED
credential). `.influx-scope.json` records the bucket and authorization IDs.

The original all-access token (`amudario's Token`, id `1134f10e0b8f4000`) is
still active but is no longer referenced by Node-RED: the MainValve flow was
removed and the Phase 6 outputs now use `9f8e7d6c5b4a3120`
(`Irrigation InfluxDB write`). Deactivate the legacy token after operator
confirmation.

## Open verification items

- P2 scoped write config (2026-10-18): the Phase 6 flow is deployed, the
  experimental MainValve tab was removed, and the four outputs use the
  workspace-owned config `9f8e7d6c5b4a3120` with the scoped write token. The
  legacy config `6b15b438c5cf6d49` was removed. Scoped writes to
  `irrigation_events` return 204; `main_valve` writes and queries are denied.
- P2 store module (2026-10-18): the updated `watering-plan-store` module and
  `003_irrigation_operations.sql` are installed; the live database is at schema
  versions 1, 2, 3. The reservation test passed (start, duplicate retry,
  conflict, stop).
- Legacy all-access token `1134f10e0b8f4000` is no longer referenced by
  Node-RED; deactivate it after operator confirmation.
- Influx token replacement (2026-10-18): active scoped read/write tokens are
  installed. The read token is in `IRRIGATION_INFLUX_TOKEN`; the write token is
  in the Node-RED config node `9f8e7d6c5b4a3120`. The four Phase 6 Influx
  outputs are re-enabled and live telemetry resumed.
- Whether the kiosk/desktop stack can be retired after the server UI replaces
  the Node-RED dashboard. Node-RED itself stays running.
- Whether the created `eclipse-mosquitto` Docker container is still wanted.
  Docker and containerd are disabled; the container and image were left in
  place.
- PostgreSQL remains enabled and running; it is not required by the Phase 6
  server.

## Read-only reproduction commands

```sh
date; uptime
cat /etc/os-release; uname -a
cat /proc/device-tree/model; echo
free -h; vmstat 1 3
df -h / /boot/firmware; df -i /
du -xh /usr /home /var/lib/influxdb 2>/dev/null | sort -h | tail
ps -eo rss,comm --sort=-rss | head -30
systemctl list-units --type=service --state=running
node-red --version; node --version; npm --version
influx version; systemctl status influxdb --no-pager
```

Do not record tokens, credentials, or device EUIs in this file.
