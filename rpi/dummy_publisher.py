# Throws fake weight samples at the server so we can verify the UI before
# the real ESP32 firmware exists.
#
# Usage on RPi:  ~/cornerweight/.venv/bin/python ~/cornerweight/dummy_publisher.py

import asyncio
import json
import math
import random
import time

import websockets

URL = "ws://127.0.0.1:8000/ws/ingest"
CORNERS = ["FL", "FR", "RL", "RR"]
BASE = {"FL": 68.0, "FR": 72.0, "RL": 65.0, "RR": 70.0}


async def main():
    async with websockets.connect(URL) as ws:
        t0 = time.time()
        while True:
            for c in CORNERS:
                # slow sine wobble + tiny noise so the UI looks alive
                t = time.time() - t0
                w = BASE[c] + 2.5 * math.sin(t * 0.6 + CORNERS.index(c)) + random.uniform(-0.2, 0.2)
                payload = {
                    "corner": c,
                    "weight_kg": round(w, 2),
                    "mac": f"aa:bb:cc:00:00:{CORNERS.index(c):02x}",
                    "rssi": random.randint(-75, -45),
                    "ts_ms": int(time.time() * 1000),
                }
                await ws.send(json.dumps(payload))
            await asyncio.sleep(0.1)  # 10 Hz, 4 samples per tick


if __name__ == "__main__":
    asyncio.run(main())
