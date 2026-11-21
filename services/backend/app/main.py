"""Retro Arcade cloud API: prototype device registry, settings, feature flags and Jellyfin status."""
import hashlib
import hmac
import os
import secrets
import uuid
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

import httpx
from fastapi import Depends, FastAPI, Header, HTTPException
from pydantic import BaseModel, Field
from sqlalchemy import JSON, DateTime, ForeignKey, String, create_engine, select
from sqlalchemy.orm import DeclarativeBase, Mapped, Session, mapped_column, relationship, sessionmaker

DATABASE_URL = os.getenv("DATABASE_URL", "sqlite:///./arcade.db")
ADMIN_API_KEY = os.getenv("ADMIN_API_KEY", "")
JELLYFIN_URL = os.getenv("JELLYFIN_URL", "http://localhost:8096").rstrip("/")
JELLYFIN_API_KEY = os.getenv("JELLYFIN_API_KEY", "")
engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False} if DATABASE_URL.startswith("sqlite") else {}, pool_pre_ping=True)
SessionLocal = sessionmaker(engine, expire_on_commit=False)

class Base(DeclarativeBase):
    pass

class Device(Base):
    __tablename__ = "devices"
    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    name: Mapped[str] = mapped_column(String(120))
    token_hash: Mapped[str] = mapped_column(String(64), nullable=False)
    firmware: Mapped[str] = mapped_column(String(60), default="unknown")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    last_seen: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    settings: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict)

class Feature(Base):
    __tablename__ = "features"
    name: Mapped[str] = mapped_column(String(80), primary_key=True)
    enabled: Mapped[bool] = mapped_column(default=True)

DEFAULT_FEATURES = {"movies": True, "tv": True, "music": True, "retro_games": False, "updates": False}
DEFAULT_SETTINGS = {"theme": "retro-arcade", "resolution": "auto", "audio_output": "hdmi", "jellyfin_enabled": True}

@asynccontextmanager
async def lifespan(app: FastAPI):
    Base.metadata.create_all(engine)
    with SessionLocal() as db:
        for name, enabled in DEFAULT_FEATURES.items():
            if not db.get(Feature, name):
                db.add(Feature(name=name, enabled=enabled))
        db.commit()
    yield

app = FastAPI(title="Retro Arcade Cloud API", version="0.1.0", lifespan=lifespan)

def get_db():
    with SessionLocal() as db:
        yield db

def admin(x_admin_key: str | None = Header(None)):
    if not ADMIN_API_KEY or not x_admin_key or not hmac.compare_digest(x_admin_key, ADMIN_API_KEY):
        raise HTTPException(401, "Invalid administrator key")

def device_auth(authorization: str | None = Header(None), db: Session = Depends(get_db)) -> Device:
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(401, "Device token required")
    token = authorization[7:]
    if not token or len(token) > 256:
        raise HTTPException(401, "Invalid token")
    token_hash = hashlib.sha256(token.encode()).hexdigest()
    device = db.scalar(select(Device).where(Device.token_hash == token_hash))
    if device is None:
        raise HTTPException(401, "Invalid device token")
    return device

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
    """Prototype provisioning: admin manually registers a device and securely transfers token."""
    token = secrets.token_urlsafe(48)
    d = Device(id=str(uuid.uuid4()), name=body.name, token_hash=hashlib.sha256(token.encode()).hexdigest(), settings=DEFAULT_SETTINGS.copy())
    db.add(d)
    db.commit()
    return {"id": d.id, "name": d.name, "device_token": token, "warning": "Token is shown only once. Store securely."}

@app.get("/api/v1/admin/devices", dependencies=[Depends(admin)])
def list_devices(db: Session = Depends(get_db)):
    return [{"id": d.id, "name": d.name, "firmware": d.firmware, "last_seen": d.last_seen} for d in db.scalars(select(Device)).all()]

@app.get("/api/v1/device/bootstrap")
def bootstrap(d: Device = Depends(device_auth), db: Session = Depends(get_db)):
    features = {f.name: f.enabled for f in db.scalars(select(Feature)).all()}
    return {"device_id": d.id, "device_name": d.name, "settings": {**DEFAULT_SETTINGS, **(d.settings or {})}, "features": features, "media": {"provider": "jellyfin", "configured": bool(JELLYFIN_API_KEY)}}

@app.post("/api/v1/device/heartbeat")
def heartbeat(body: Heartbeat, d: Device = Depends(device_auth), db: Session = Depends(get_db)):
    d.firmware = body.firmware
    d.last_seen = datetime.now(timezone.utc)
    db.add(d)
    db.commit()
    return {"ok": True}

@app.patch("/api/v1/admin/devices/{device_id}/settings", dependencies=[Depends(admin)])
def update_settings(device_id: str, body: SettingsPatch, db: Session = Depends(get_db)):
    d = db.get(Device, device_id)
    if not d:
        raise HTTPException(404, "Device not found")
    d.settings = {**(d.settings or {}), **body.model_dump(exclude_none=True)}
    db.add(d)
    db.commit()
    return d.settings

@app.patch("/api/v1/admin/features/{feature_name}", dependencies=[Depends(admin)])
def update_feature(feature_name: str, body: FeaturePatch, db: Session = Depends(get_db)):
    f = db.get(Feature, feature_name)
    if not f:
        raise HTTPException(404, "Unknown feature")
    f.enabled = body.enabled
    db.add(f)
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
