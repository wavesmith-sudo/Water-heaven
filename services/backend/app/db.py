"""Database engine, models and session dependency."""
import os
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import JSON, DateTime, ForeignKey, String, create_engine
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, sessionmaker

DATABASE_URL = os.getenv("DATABASE_URL", "sqlite:///./arcade.db")
engine = create_engine(
    DATABASE_URL,
    connect_args={"check_same_thread": False} if DATABASE_URL.startswith("sqlite") else {},
    pool_pre_ping=True,
)
SessionLocal = sessionmaker(engine, expire_on_commit=False)


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def as_utc(value: datetime | None) -> datetime | None:
    """SQLite returns naive datetimes; treat stored values as UTC."""
    if value is None or value.tzinfo is not None:
        return value
    return value.replace(tzinfo=timezone.utc)


class Base(DeclarativeBase):
    pass


class Device(Base):
    __tablename__ = "devices"
    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    name: Mapped[str] = mapped_column(String(120))
    token_hash: Mapped[str] = mapped_column(String(64), nullable=False, index=True)
    firmware: Mapped[str] = mapped_column(String(60), default="unknown")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)
    last_seen: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    settings: Mapped[dict[str, Any]] = mapped_column(JSON, default=dict)
    revoked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


class Feature(Base):
    __tablename__ = "features"
    name: Mapped[str] = mapped_column(String(80), primary_key=True)
    enabled: Mapped[bool] = mapped_column(default=True)


class PairingRequest(Base):
    """A device waiting to be approved. The user code is shown on the TV; the device code is the
    device's secret for polling. Both are stored hashed."""

    __tablename__ = "pairing_requests"
    id: Mapped[str] = mapped_column(String(36), primary_key=True)
    user_code_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device_code_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device_name: Mapped[str] = mapped_column(String(120))
    firmware: Mapped[str] = mapped_column(String(60), default="unknown")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    approved_device_id: Mapped[str | None] = mapped_column(ForeignKey("devices.id"), nullable=True)
    claimed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)


def get_db():
    with SessionLocal() as db:
        yield db
