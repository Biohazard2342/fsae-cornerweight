#!/usr/bin/env bash
# Start the serial→websocket bridge as a background process.
set -e
cd ~/cornerweight
pkill -f serial_bridge.py >/dev/null 2>&1 || true
sleep 0.3
nohup ./.venv/bin/python ~/cornerweight/bridge/serial_bridge.py \
  </dev/null >~/cornerweight/bridge.log 2>&1 &
disown $! 2>/dev/null || true
sleep 1
echo "--- bridge log ---"
tail -n 10 ~/cornerweight/bridge.log
