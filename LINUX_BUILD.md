# Meridian 59 Server - Linux Build Instructions

## Prerequisites (Ubuntu/Debian)

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib flex bison \
    libjansson-dev libjansson-dev:i386 \
    libpq-dev libpq-dev:i386 \
    ncat python3
```

## Build

```bash
make -f makefile.linux
```

Builds: blakserv, blakcomp, rscmerge, all KOD/BOF/RSC/RSB files, copies rooms.

## Configuration

```bash
cd run/server
cp blakserv.cfg-linux blakserv.cfg
mkdir -p savegame
```

Edit `blakserv.cfg` - important settings:
```ini
[Path]
Rooms                rooms/

[Socket]
MaintenanceMask      ::ffff:127.0.0.1;::1

[Channel]
Flush                Yes

[Memory]
SizeClassHash        199999

[Resource]
RscSpec              *.rsb

[MySQL]
Enabled     Yes
Host        127.0.0.1
Port        5432
Username    blakserv
Password    blaks3kr1t
Database    meridian59
```

Set `[MySQL] Enabled No` to run without database.

## PostgreSQL Setup

```bash
cd docker/postgres
docker compose up -d
```

This starts a PostgreSQL 16 container on port 5432. The server creates all ~46 tables automatically on first connect.

Test with:
```bash
docker exec m59-postgres psql -U blakserv -d meridian59 -c "\dt"
```

## Run

```bash
cd run/server
./blakserv &        # background
./blakserv          # foreground (Ctrl+C to stop)
```

Ports: **5959** (game), **9998** (admin maintenance)

## Admin Commands

```bash
bash blakadmin.sh show status
bash blakadmin.sh show accounts
bash blakadmin.sh who
bash blakadmin.sh save game
bash blakadmin.sh send o 0 updatedatabase
bash blakadmin.sh send o 0 getuniqueips
bash blakadmin.sh create account admin username password email
bash blakadmin.sh shutdown          # save + stop server

# Interactive:
bash blakadmin.sh
blakadm> show status
blakadm> bye
```

## Logs

```bash
tail -f run/server/channel/*.txt
```

Files: `debug.txt`, `error.txt`, `log.txt`, `god.txt`, `admin.txt`

## Architecture

Single-threaded epoll main loop (based on vanilla Meridian59 repo), with a separate
PostgreSQL writer thread for async database operations.

- `osd_linux.c/h` - OS-dependent types and stubs
- `osd_epoll.c` - Main loop (epoll socket multiplexing + timers)
- `database_pg.c/h` - PostgreSQL database layer (replaces MySQL)
- `main.c` - Unified Windows/Linux main with `#ifdef`
- Admin via maintenance port only (no console `-i` mode)
