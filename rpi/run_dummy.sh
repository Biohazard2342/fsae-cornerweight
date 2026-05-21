#!/usr/bin/env bash
pkill -f dummy_publisher.py >/dev/null 2>&1 || true
sleep 0.3
nohup ~/cornerweight/.venv/bin/python ~/cornerweight/dummy_publisher.py \
  </dev/null >~/cornerweight/dummy.log 2>&1 &
disown $! 2>/dev/null || true
sleep 1.5
echo "--- dummy log ---"
tail -n 5 ~/cornerweight/dummy.log
echo "--- snapshot ---"
curl -s http://127.0.0.1:8000/api/snapshot
echo
