#!/usr/bin/env bash
# Starts the simulator in the background, detached from this shell.
# All stdout/stderr goes to log.txt. Exiting the shell will not stop it.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PIDFILE="$(pwd)/simulator.pid"

if [[ -f "$PIDFILE" ]] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
  echo "Already running (pid $(cat "$PIDFILE")). Logs: $(pwd)/log.txt"
  exit 0
fi

nohup node server.js > log.txt 2>&1 &
echo $! > "$PIDFILE"
disown

echo "Started simulator (pid $!). Logs: $(pwd)/log.txt"
