"""Device pairing, modelled on the OAuth 2.0 device authorization grant (RFC 8628).

1. The device calls POST /api/v1/pairing/start and shows the returned user code on the TV.
2. The owner approves that code (today: admin API; later: the web dashboard).
3. The device polls POST /api/v1/pairing/poll with its secret device code until it receives its
   device token, exactly once.
"""
import secrets
import uuid
from collections import deque
from datetime import timedelta
from threading import Lock
from time import monotonic

from fastapi import APIRouter, Depends, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from sqlalchemy import delete, func, select
from sqlalchemy.orm import Session

from .db import Device, PairingRequest, as_utc, get_db, utcnow
from .security import admin, new_device_token, new_user_code, normalize_user_code, sha256

PAIRING_TTL = timedelta(minutes=10)
POLL_INTERVAL_S = 5
MAX_PENDING = 200          # global cap on outstanding requests
START_LIMIT_PER_MIN = 10   # per client address


class _RateLimiter:
    """Small in-process sliding-window limiter. Put a real one at the reverse proxy for production."""

    def __init__(self, limit: int, window_s: float = 60.0):
        self.limit, self.window = limit, window_s
        self.hits: dict[str, deque] = {}
        self.lock = Lock()

    def allow(self, key: str) -> bool:
        now = monotonic()
        with self.lock:
            q = self.hits.setdefault(key, deque())
            while q and now - q[0] > self.window:
                q.popleft()
            if len(q) >= self.limit:
                return False
            q.append(now)
            return True

    def reset(self):
        with self.lock:
            self.hits.clear()


start_limiter = _RateLimiter(START_LIMIT_PER_MIN)

router = APIRouter()
DEFAULT_SETTINGS_REF: dict = {}  # filled by main to avoid an import cycle


class PairingStart(BaseModel):
    device_name: str = Field(min_length=1, max_length=120)
    firmware: str = Field("unknown", min_length=1, max_length=60)


class PairingPoll(BaseModel):
    device_code: str = Field(min_length=20, max_length=128)


class PairingApprove(BaseModel):
    name: str | None = Field(None, min_length=1, max_length=120)


def _purge_expired(db: Session):
    db.execute(delete(PairingRequest).where(PairingRequest.expires_at < utcnow(), PairingRequest.approved_device_id.is_(None)))


@router.post("/api/v1/pairing/start", status_code=201, tags=["pairing"])
def pairing_start(body: PairingStart, request: Request, db: Session = Depends(get_db)):
    client = request.client.host if request.client else "unknown"
    if not start_limiter.allow(client):
        raise HTTPException(429, "Too many pairing attempts; wait a minute")
    _purge_expired(db)
    if db.scalar(select(func.count()).select_from(PairingRequest).where(PairingRequest.claimed_at.is_(None))) >= MAX_PENDING:
        raise HTTPException(503, "Pairing temporarily unavailable")
    device_code = secrets.token_urlsafe(32)
    for _ in range(5):
        user_code = new_user_code()
        if not db.scalar(select(PairingRequest).where(PairingRequest.user_code_hash == sha256(user_code))):
            break
    else:
        raise HTTPException(503, "Could not allocate a pairing code")
    db.add(PairingRequest(
        id=str(uuid.uuid4()), user_code_hash=sha256(user_code), device_code_hash=sha256(device_code),
        device_name=body.device_name, firmware=body.firmware, expires_at=utcnow() + PAIRING_TTL,
    ))
    db.commit()
    return {"user_code": user_code, "device_code": device_code,
            "expires_in": int(PAIRING_TTL.total_seconds()), "interval": POLL_INTERVAL_S}


@router.post("/api/v1/pairing/poll", tags=["pairing"],
             responses={202: {"description": "authorization_pending"}, 404: {}, 410: {"description": "expired_token"}})
def pairing_poll(body: PairingPoll, db: Session = Depends(get_db)):
    req = db.scalar(select(PairingRequest).where(PairingRequest.device_code_hash == sha256(body.device_code)))
    if req is None or req.claimed_at is not None:
        raise HTTPException(404, "Unknown or already used device code")
    if req.approved_device_id is None:
        if as_utc(req.expires_at) < utcnow():
            raise HTTPException(410, "expired_token")
        return JSONResponse(status_code=202, content={"status": "authorization_pending", "interval": POLL_INTERVAL_S})
    # The token is minted here, at hand-over, so it is never stored anywhere in plain text.
    device = db.get(Device, req.approved_device_id)
    token = new_device_token()
    device.token_hash = sha256(token)
    req.claimed_at = utcnow()
    db.commit()
    return {"status": "approved", "device_id": device.id, "device_name": device.name, "device_token": token}


@router.post("/api/v1/admin/pairing/{user_code}/approve", dependencies=[Depends(admin)], status_code=201, tags=["admin"])
def pairing_approve(user_code: str, body: PairingApprove | None = None, db: Session = Depends(get_db)):
    req = db.scalar(select(PairingRequest).where(PairingRequest.user_code_hash == sha256(normalize_user_code(user_code))))
    if req is None or as_utc(req.expires_at) < utcnow():
        raise HTTPException(404, "Unknown or expired pairing code")
    if req.approved_device_id is not None:
        raise HTTPException(409, "Pairing code already approved")
    # Placeholder hash of a discarded secret: unusable until the device claims its real token.
    device = Device(id=str(uuid.uuid4()), name=(body.name if body and body.name else req.device_name),
                    token_hash=sha256(secrets.token_hex(32)), firmware=req.firmware, settings=dict(DEFAULT_SETTINGS_REF))
    db.add(device)
    db.flush()
    req.approved_device_id = device.id
    db.commit()
    return {"id": device.id, "name": device.name}


@router.get("/api/v1/admin/pairing", dependencies=[Depends(admin)], tags=["admin"])
def pairing_pending(db: Session = Depends(get_db)):
    """Pending requests, for an admin UI. User codes are stored hashed, so only names and times are listed."""
    _purge_expired(db)
    db.commit()
    rows = db.scalars(select(PairingRequest).where(PairingRequest.approved_device_id.is_(None))).all()
    return [{"id": r.id, "device_name": r.device_name, "firmware": r.firmware, "expires_at": r.expires_at} for r in rows]
