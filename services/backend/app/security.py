"""Authentication dependencies and token helpers."""
import hashlib
import hmac
import os
import secrets

from fastapi import Depends, Header, HTTPException
from sqlalchemy import select
from sqlalchemy.orm import Session

from .db import Device, get_db

ADMIN_API_KEY = os.getenv("ADMIN_API_KEY", "")
# Unambiguous on a TV screen: no 0/O, 1/I/L, 5/S, 2/Z.
USER_CODE_ALPHABET = "ABCDEFGHJKMNPQRTUVWXY346789"


def sha256(value: str) -> str:
    return hashlib.sha256(value.encode()).hexdigest()


def new_device_token() -> str:
    return secrets.token_urlsafe(48)


def new_user_code() -> str:
    raw = "".join(secrets.choice(USER_CODE_ALPHABET) for _ in range(8))
    return f"{raw[:4]}-{raw[4:]}"


def normalize_user_code(code: str) -> str:
    raw = "".join(ch for ch in code.upper() if ch.isalnum())
    return f"{raw[:4]}-{raw[4:]}" if len(raw) == 8 else raw


def admin(x_admin_key: str | None = Header(None)):
    if not ADMIN_API_KEY or not x_admin_key or not hmac.compare_digest(x_admin_key, ADMIN_API_KEY):
        raise HTTPException(401, "Invalid administrator key")


def device_auth(authorization: str | None = Header(None), db: Session = Depends(get_db)) -> Device:
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(401, "Device token required")
    token = authorization[7:]
    if not token or len(token) > 256:
        raise HTTPException(401, "Invalid token")
    device = db.scalar(select(Device).where(Device.token_hash == sha256(token)))
    if device is None or device.revoked_at is not None:
        raise HTTPException(401, "Invalid device token")
    return device
