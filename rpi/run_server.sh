#!/usr/bin/env bash
set -e
cd ~/cornerweight
pkill -f "uvicorn server.main" >/dev/null 2>&1 || true
sleep 0.5
nohup ./.venv/bin/uvicorn server.main:app --host 0.0.0.0 --port 8000 \
  </dev/null >~/cornerweight/uvicorn.log 2>&1 &
disown $! 2>/dev/null || true
sleep 2
echo "--- listening ---"
ss -ltn | awk 'NR==1 || /:8000/'
echo "--- log ---"
tail -n 20 ~/cornerweight/uvicorn.log
echo "--- snapshot ---"
curl -s http://127.0.0.1:8000/api/snapshot
echo
