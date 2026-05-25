#!/usr/bin/env bash
# Fresh-RPi bootstrap for FSAE corner weight system.
# Pulls the repo and reproduces the full runtime (server + bridge + systemd).
# Idempotent — safe to re-run.
set -euo pipefail

REPO_URL="https://github.com/Biohazard2342/fsae-cornerweight.git"
SRC=~/cornerweight-src
APP=~/cornerweight

echo "=== 1/5: apt packages ==="
sudo -n apt-get update -qq
sudo -n apt-get install -y -qq git python3-venv python3-pip curl

echo "=== 2/5: clone / pull repo ==="
if [ -d "$SRC/.git" ]; then
    git -C "$SRC" pull --ff-only
else
    git clone --depth 1 "$REPO_URL" "$SRC"
fi

echo "=== 3/5: lay out app directory ==="
mkdir -p "$APP"/{server,static,bridge}
cp -f "$SRC"/rpi/server/main.py        "$APP"/server/
cp -f "$SRC"/rpi/static/index.html     "$APP"/static/
cp -f "$SRC"/rpi/static/logo.png       "$APP"/static/ 2>/dev/null || true
cp -f "$SRC"/rpi/bridge/serial_bridge.py "$APP"/bridge/

echo "=== 4/6: venv + Python deps ==="
if [ ! -x "$APP/.venv/bin/python3" ]; then
    python3 -m venv "$APP/.venv"
fi
"$APP/.venv/bin/pip" install --quiet --upgrade pip
"$APP/.venv/bin/pip" install --quiet fastapi 'uvicorn[standard]' websockets pyserial platformio

echo "=== 5/6: ESP32 firmware sources ==="
mkdir -p "$APP/firmware/src"
cp -f "$SRC/platformio.ini" "$APP/firmware/platformio.ini"
cp -f "$SRC/src/main.cpp"   "$APP/firmware/src/main.cpp"

echo "=== 6/6: systemd units ==="
sudo -n install -m 644 "$SRC"/rpi/systemd/cornerweight-server.service /etc/systemd/system/
sudo -n install -m 644 "$SRC"/rpi/systemd/cornerweight-bridge.service /etc/systemd/system/
sudo -n systemctl daemon-reload
sudo -n systemctl enable --now cornerweight-server cornerweight-bridge

echo
echo "=== status ==="
sudo -n systemctl is-active cornerweight-server cornerweight-bridge
echo "=== snapshot ==="
sleep 1
curl -s http://127.0.0.1:8000/api/snapshot | head -c 300
echo
echo "=== DONE ==="
