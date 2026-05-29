# FSAE Corner Weight - FastAPI WebSocket Server
#
# Two WebSocket endpoints:
#   /ws/ingest   <- ESP32 pushes weight samples (JSON)
#   /ws/display  -> browsers receive live updates
#
# Static UI served at /

from __future__ import annotations

import asyncio
import json
import time
from pathlib import Path
from typing import Optional

from typing import Union

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

CORNERS = ("FL", "FR", "RL", "RR")

STATIC_DIR = Path(__file__).resolve().parent.parent / "static"

app = FastAPI(title="FSAE Corner Weight")


class Sample(BaseModel):
    corner: str = Field(..., description="FL | FR | RL | RR")
    # weight_kg is optional so smoke-test / unmapped-payload samples can flow
    # purely as connectivity heartbeats without polluting the UI with fake numbers.
    weight_kg: Optional[float] = None
    mac: Optional[str] = None
    rssi: Optional[int] = None
    # raw is hex string in BLE firmware, plain int in HX711 firmware — accept either
    raw: Optional[Union[str, int]] = None
    ts_ms: Optional[int] = None


class Hub:
    """Holds latest sample per corner and fans out to display clients."""

    def __init__(self) -> None:
        self.latest: dict[str, dict] = {}
        self.display_clients: set[WebSocket] = set()
        self.ingest_clients: set[WebSocket] = set()
        self.last_ingest_ms: Optional[int] = None
        self.lock = asyncio.Lock()

    def ingest_status(self) -> dict:
        return {
            "connected": len(self.ingest_clients),
            "last_ms": self.last_ingest_ms,
        }

    def display_status(self) -> dict:
        return {"count": len(self.display_clients)}

    def snapshot(self) -> dict:
        return {
            "corners": self.latest,
            "ingest": self.ingest_status(),
            "display": self.display_status(),
            "server_ts": int(time.time() * 1000),
        }

    async def _broadcast(self, msg: dict) -> None:
        stale = []
        for ws in self.display_clients:
            try:
                await ws.send_json(msg)
            except Exception:
                stale.append(ws)
        for ws in stale:
            self.display_clients.discard(ws)

    async def publish(self, sample: Sample) -> None:
        payload = sample.model_dump()
        payload["server_ts"] = int(time.time() * 1000)
        async with self.lock:
            self.latest[sample.corner] = payload
            self.last_ingest_ms = payload["server_ts"]
            await self._broadcast({"type": "sample", "data": payload})

    async def register_display(self, ws: WebSocket) -> None:
        async with self.lock:
            self.display_clients.add(ws)
            await ws.send_json({"type": "snapshot", "data": self.snapshot()})
            await self._broadcast({"type": "display_status", "data": self.display_status()})

    async def drop_display(self, ws: WebSocket) -> None:
        async with self.lock:
            self.display_clients.discard(ws)
            await self._broadcast({"type": "display_status", "data": self.display_status()})

    async def register_ingest(self, ws: WebSocket) -> None:
        async with self.lock:
            self.ingest_clients.add(ws)
            await self._broadcast({"type": "ingest_status", "data": self.ingest_status()})

    async def drop_ingest(self, ws: WebSocket) -> None:
        async with self.lock:
            self.ingest_clients.discard(ws)
            await self._broadcast({"type": "ingest_status", "data": self.ingest_status()})


hub = Hub()


@app.get("/api/snapshot")
async def snapshot():
    return hub.snapshot()


@app.websocket("/ws/ingest")
async def ws_ingest(ws: WebSocket):
    await ws.accept()
    await hub.register_ingest(ws)
    try:
        while True:
            raw = await ws.receive_text()
            try:
                data = json.loads(raw)
                sample = Sample.model_validate(data)
            except Exception as e:
                await ws.send_json({"error": str(e)})
                continue
            if sample.corner not in CORNERS:
                await ws.send_json({"error": f"unknown corner {sample.corner}"})
                continue
            await hub.publish(sample)
    except WebSocketDisconnect:
        pass
    finally:
        await hub.drop_ingest(ws)


@app.websocket("/ws/display")
async def ws_display(ws: WebSocket):
    await ws.accept()
    await hub.register_display(ws)
    try:
        while True:
            # browsers don't need to send anything; just keep the socket open
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        await hub.drop_display(ws)


# Static UI at /
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")


@app.get("/")
async def index():
    return FileResponse(str(STATIC_DIR / "index.html"))
