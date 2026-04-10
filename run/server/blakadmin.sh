#!/bin/bash
# blakadmin.sh - Admin tool for Meridian 59 Linux server
# Sends commands to the maintenance port (default 9998)

HOST="${BLAKSERV_HOST:-127.0.0.1}"
PORT="${BLAKSERV_PORT:-9998}"

if [ $# -eq 0 ]; then
    # Interactive mode
    echo "Meridian 59 Admin Console ($HOST:$PORT)"
    echo "Type commands, 'bye' to exit"
    echo "---"
    while true; do
        read -p "blakadm> " cmd
        [ -z "$cmd" ] && continue
        [ "$cmd" = "bye" ] || [ "$cmd" = "quit" ] || [ "$cmd" = "exit" ] && break
        python3 -c "
import socket, time
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5)
try:
    s.connect(('$HOST', $PORT))
    s.sendall(b'$cmd\r\n')
    time.sleep(0.3)
    while True:
        try:
            data = s.recv(8192)
            if not data: break
            text = data.decode('latin-1')
            for line in text.splitlines():
                if not line.startswith('> '): print(line)
        except socket.timeout:
            break
except Exception as e:
    print(f'Error: {e}')
finally:
    s.close()
"
    done
else
    # Single command mode
    CMD="$*"
    python3 -c "
import socket, time, sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5)
try:
    s.connect(('$HOST', $PORT))
    s.sendall(b'$CMD\r\n')
    time.sleep(0.3)
    while True:
        try:
            data = s.recv(8192)
            if not data: break
            text = data.decode('latin-1')
            for line in text.splitlines():
                if not line.startswith('> '): print(line)
        except socket.timeout:
            break
except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
finally:
    s.close()
"
fi
