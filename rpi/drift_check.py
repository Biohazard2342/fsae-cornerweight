#!/usr/bin/env python3
"""Sample weight + raw every second for 10 s to check drift on an empty scale."""
import json
import time
import urllib.request


URL = "http://127.0.0.1:8000/api/snapshot"

print("--- drift monitor (10s) — keep scale empty and still ---")
first_raw = None
for i in range(10):
    r = urllib.request.urlopen(URL).read()
    d = json.loads(r)
    fl = d["corners"].get("FL", {})
    raw = fl.get("raw")
    w   = fl.get("weight_kg")
    if first_raw is None and raw is not None:
        first_raw = raw
    delta = (raw - first_raw) if (raw is not None and first_raw is not None) else 0
    w_str = f"{w:.4f}" if isinstance(w, (int, float)) else "--"
    print(f"t={i}s  raw={raw}  delta={delta:+}  weight={w_str} kg")
    time.sleep(1)
