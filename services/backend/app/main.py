"""Retro Arcade cloud API: device registry and pairing, settings, feature flags and Jellyfin status."""
import os
import uuid
from contextlib import asynccontextmanager

import httpx
from fastapi import Depends, FastAPI, HTTPException
from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.orm import Session

from . import pairing
from .db import Device, Feature, SessionLocal, get_db, utcnow
from .migrations import upgrade_database
from .security import admin, device_auth, new_device_token, sha256

JELLYFIN_URL = os.getenv("JELLYFIN_URL", "http://localhost:8096").rstrip("/")
JELLYFIN_API_KEY = os.getenv("JELLYFIN_API_KEY", "")

DEFAULT_FEATURES = {"movies": True, "tv": True, "music": True, "retro_games": False, "updates": False}
DEFAULT_SETTINGS = {"theme": "retro-arcade", "resolution": "auto", "audio_output": "hdmi", "jellyfin_enabled": True}
pairing.DEFAULT_SETTINGS_REF.update(DEFAULT_SETTINGS)


@asynccontextmanager
async def lifespan(app: FastAPI):
    upgrade_database()
    with SessionLocal() as db:
        for name, enabled in DEFAULT_FEATURES.items():
            if not db.get(Feature, name):
                db.add(Feature(name=name, enabled=enabled))
        db.commit()
    yield


app = FastAPI(title="Retro Arcade Cloud API", version="0.2.0", lifespan=lifespan)
app.include_router(pairing.router)


class DeviceCreate(BaseModel):
    name: str = Field(min_length=1, max_length=120)


class SettingsPatch(BaseModel):
    theme: str | None = Field(None, pattern="^(retro-arcade|minimal-tv|modern-media)$")
    resolution: str | None = Field(None, pattern="^(auto|1080p|4k)$")
    audio_output: str | None = Field(None, pattern="^(hdmi|bluetooth)$")
    jellyfin_enabled: bool | None = None


class FeaturePatch(BaseModel):
    enabled: bool


class Heartbeat(BaseModel):
    firmware: str = Field(min_length=1, max_length=60)


@app.get("/health")
def health():
    return {"status": "ok", "service": "retro-arcade-api"}


@app.post("/api/v1/admin/devices", dependencies=[Depends(admin)], status_code=201)
def register_device(body: DeviceCreate, db: Session = Depends(get_db)):
    """Manual provisioning: prefer the pairing flow. The token is shown once; transfer it securely."""
    token = new_device_token()
    d = Device(id=str(uuid.uuid4()), name=body.name, token_hash=sha256(token), settings=DEFAULT_SETTINGS.copy())
    db.add(d)
    db.commit()
    return {"id": d.id, "name": d.name, "device_token": token, "warning": "Token is shown only once. Store securely."}


@app.get("/api/v1/admin/devices", dependencies=[Depends(admin)])
def list_devices(db: Session = Depends(get_db)):
    return [{"id": d.id, "name": d.name, "firmware": d.firmware, "last_seen": d.last_seen,
             "revoked": d.revoked_at is not None} for d in db.scalars(select(Device)).all()]


@app.post("/api/v1/admin/devices/{device_id}/revoke", dependencies=[Depends(admin)])
def revoke_device(device_id: str, db: Session = Depends(get_db)):
    """Permanently disables the device's token. The device must pair again."""
    d = db.get(Device, device_id)
    if not d:
        raise HTTPException(404, "Device not found")
    if d.revoked_at is None:
        d.revoked_at = utcnow()
        db.commit()
    return {"id": d.id, "revoked": True}


@app.get("/api/v1/device/bootstrap")
def bootstrap(d: Device = Depends(device_auth), db: Session = Depends(get_db)):
    features = {f.name: f.enabled for f in db.scalars(select(Feature)).all()}
    return {"device_id": d.id, "device_name": d.name, "settings": {**DEFAULT_SETTINGS, **(d.settings or {})},
            "features": features, "media": {"provider": "jellyfin", "configured": bool(JELLYFIN_API_KEY)}}


@app.post("/api/v1/device/heartbeat")
def heartbeat(body: Heartbeat, d: Device = Depends(device_auth), db: Session = Depends(get_db)):
    d.firmware = body.firmware
    d.last_seen = utcnow()
    db.commit()
    return {"ok": True}


@app.post("/api/v1/device/token/rotate")
def rotate_token(d: Device = Depends(device_auth), db: Session = Depends(get_db)):
    """Issues a new token and invalidates the one used for this request."""
    token = new_device_token()
    d.token_hash = sha256(token)
    db.commit()
    return {"device_token": token}


@app.patch("/api/v1/admin/devices/{device_id}/settings", dependencies=[Depends(admin)])
def update_settings(device_id: str, body: SettingsPatch, db: Session = Depends(get_db)):
    d = db.get(Device, device_id)
    if not d:
        raise HTTPException(404, "Device not found")
    d.settings = {**(d.settings or {}), **body.model_dump(exclude_none=True)}
    db.commit()
    return d.settings


@app.patch("/api/v1/admin/features/{feature_name}", dependencies=[Depends(admin)])
def update_feature(feature_name: str, body: FeaturePatch, db: Session = Depends(get_db)):
    f = db.get(Feature, feature_name)
    if not f:
        raise HTTPException(404, "Unknown feature")
    f.enabled = body.enabled
    db.commit()
    return {"name": f.name, "enabled": f.enabled}


@app.get("/api/v1/admin/jellyfin/status", dependencies=[Depends(admin)])
async def jellyfin_status():
    if not JELLYFIN_API_KEY:
        return {"configured": False, "reachable": False}
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.get(f"{JELLYFIN_URL}/System/Info/Public")
            r.raise_for_status()
            return {"configured": True, "reachable": True, "server_name": r.json().get("ServerName")}
    except (httpx.HTTPError, ValueError):
        return {"configured": True, "reachable": False}
