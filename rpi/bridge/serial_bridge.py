# FSAE Corner Weight - Serial → WebSocket Bridge
#
# Reads JSON lines from the ESP32 (USB CDC, /dev/ttyACM0 by default) and
# forwards each one verbatim to the local server at /ws/ingest.
#
# Non-JSON lines (e.g. DUMP_ALL human-readable output, boot logs) are skipped
# silently — handy while reverse-engineering packets.
#
# Robust to: ESP32 unplug/replug, server restart, baud noise.

import json
import os
import sys
import time

import serial
from websockets.sync.client import connect as ws_connect

SERIAL_DEVICE = os.environ.get("ESP32_SERIAL", "/dev/ttyACM0")
SERIAL_BAUD   = int(os.environ.get("ESP32_BAUD",  "115200"))
WS_URL        = os.environ.get("INGEST_WS",       "ws://127.0.0.1:8000/ws/ingest")
RETRY_S       = 2.0


def run_once():
    print(f"[bridge] opening serial {SERIAL_DEVICE} @ {SERIAL_BAUD}", flush=True)
    with serial.Serial(SERIAL_DEVICE, SERIAL_BAUD, timeout=1) as s:
        print(f"[bridge] connecting ws {WS_URL}", flush=True)
        with ws_connect(WS_URL, open_timeout=5) as ws:
            print("[bridge] forwarding...", flush=True)
            sent = 0
            while True:
                raw = s.readline()
                if not raw:
                    continue  # serial readline timeout — just loop
                line = raw.decode("utf-8", errors="replace").strip()
                if not line or not line.startswith("{"):
                    continue
                try:
                    json.loads(line)  # validate before sending
                except json.JSONDecodeError:
                    continue
                ws.send(line)
                sent += 1
                if sent % 100 == 0:
                    print(f"[bridge] forwarded {sent} samples", flush=True)


def main():
    while True:
        try:
            run_once()
        except KeyboardInterrupt:
            print("[bridge] interrupted, exit", flush=True)
            return 0
        except Exception as e:
            print(f"[bridge] {type(e).__name__}: {e}  (retry in {RETRY_S}s)", flush=True)
            time.sleep(RETRY_S)


if __name__ == "__main__":
    sys.exit(main() or 0)
