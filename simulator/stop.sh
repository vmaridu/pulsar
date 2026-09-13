#!/usr/bin/env bash
# Stops the background simulator started by start.sh.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PIDFILE="$(pwd)/simulator.pid"

if [[ ! -f "$PIDFILE" ]] || ! kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
  rm -f "$PIDFILE"
  echo "Simulator is not running."
  exit 0
fi

pid="$(cat "$PIDFILE")"
kill "$pid"
rm -f "$PIDFILE"
echo "Stopped simulator (pid $pid)."
