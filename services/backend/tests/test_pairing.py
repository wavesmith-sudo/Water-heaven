"""Device pairing (RFC 8628-style) and token lifecycle."""
from datetime import timedelta

import pytest

from app import pairing
from app.db import PairingRequest, SessionLocal, utcnow
from conftest import ADMIN

START = "/api/v1/pairing/start"
POLL = "/api/v1/pairing/poll"


def approve(client, code, **body):
    return client.post(f"/api/v1/admin/pairing/{code}/approve", headers=ADMIN, json=body or None)


def start(client, name="Living Room"):
    r = client.post(START, json={"device_name": name, "firmware": "0.4.0"})
    assert r.status_code == 201
    return r.json()


def test_full_pairing_flow(client):
    s = start(client)
    assert len(s["user_code"]) == 9 and s["user_code"][4] == "-"
    assert s["interval"] > 0 and s["expires_in"] == 600

    r = client.post(POLL, json={"device_code": s["device_code"]})
    assert r.status_code == 202 and r.json()["status"] == "authorization_pending"

    a = approve(client, s["user_code"].lower().replace("-", " "), name="Den TV")
    assert a.status_code == 201

    r = client.post(POLL, json={"device_code": s["device_code"]})
    assert r.status_code == 200
    body = r.json()
    assert body["device_id"] == a.json()["id"] and body["device_name"] == "Den TV"
    headers = {"Authorization": "Bearer " + body["device_token"]}
    boot = client.get("/api/v1/device/bootstrap", headers=headers).json()
    assert boot["device_id"] == body["device_id"] and boot["settings"]["theme"] == "retro-arcade"

    # The token is handed over exactly once.
    assert client.post(POLL, json={"device_code": s["device_code"]}).status_code == 404


def test_approved_device_is_unusable_before_claim(client):
    s = start(client)
    approve(client, s["user_code"])
    devices = client.get("/api/v1/admin/devices", headers=ADMIN).json()
    assert any(d["name"] == "Living Room" for d in devices)
    # The device token is minted at claim time; approval alone issues nothing usable.
    assert "device_token" not in str(client.get("/api/v1/admin/devices", headers=ADMIN).json())


def test_codes_are_stored_hashed(client):
    s = start(client)
    with SessionLocal() as db:
        rows = db.query(PairingRequest).all()
    flat = " ".join(f"{r.user_code_hash} {r.device_code_hash}" for r in rows)
    assert s["user_code"] not in flat and s["device_code"] not in flat


def test_approve_requires_admin_and_valid_code(client):
    s = start(client)
    assert client.post(f"/api/v1/admin/pairing/{s['user_code']}/approve").status_code == 401
    assert approve(client, "AAAA-AAAA").status_code == 404
    assert approve(client, s["user_code"]).status_code == 201
    assert approve(client, s["user_code"]).status_code == 409


def test_expired_code(client):
    s = start(client)
    with SessionLocal() as db:
        for r in db.query(PairingRequest).all():
            r.expires_at = utcnow() - timedelta(seconds=1)
        db.commit()
    assert client.post(POLL, json={"device_code": s["device_code"]}).status_code == 410
    assert approve(client, s["user_code"]).status_code == 404


@pytest.mark.parametrize("code", ["x" * 19, "x" * 129])
def test_poll_validation(client, code):
    assert client.post(POLL, json={"device_code": code}).status_code == 422


def test_unknown_device_code(client):
    assert client.post(POLL, json={"device_code": "n" * 43}).status_code == 404


def test_start_is_rate_limited(client):
    for _ in range(pairing.START_LIMIT_PER_MIN):
        start(client)
    r = client.post(START, json={"device_name": "flood"})
    assert r.status_code == 429


def test_pending_list_hides_codes(client):
    s = start(client, name="Bedroom")
    rows = client.get("/api/v1/admin/pairing", headers=ADMIN).json()
    assert any(r["device_name"] == "Bedroom" for r in rows)
    assert s["user_code"] not in str(rows)
    assert client.get("/api/v1/admin/pairing").status_code == 401


def test_revocation_blocks_device(client, device):
    assert client.get("/api/v1/device/bootstrap", headers=device["headers"]).status_code == 200
    r = client.post(f"/api/v1/admin/devices/{device['id']}/revoke", headers=ADMIN)
    assert r.status_code == 200
    assert client.get("/api/v1/device/bootstrap", headers=device["headers"]).status_code == 401
    assert client.post("/api/v1/device/heartbeat", headers=device["headers"], json={"firmware": "1"}).status_code == 401
    listing = client.get("/api/v1/admin/devices", headers=ADMIN).json()
    assert next(d for d in listing if d["id"] == device["id"])["revoked"] is True
    assert client.post("/api/v1/admin/devices/nope/revoke", headers=ADMIN).status_code == 404
    assert client.post(f"/api/v1/admin/devices/{device['id']}/revoke").status_code == 401


def test_token_rotation(client, device):
    r = client.post("/api/v1/device/token/rotate", headers=device["headers"])
    assert r.status_code == 200
    new = {"Authorization": "Bearer " + r.json()["device_token"]}
    assert client.get("/api/v1/device/bootstrap", headers=device["headers"]).status_code == 401
    assert client.get("/api/v1/device/bootstrap", headers=new).status_code == 200
