"""Negative authentication and validation tests (docs/SECURITY.md, Milestone 2 groundwork)."""
import pytest

from conftest import ADMIN

ADMIN_ROUTES = [
    ("get", "/api/v1/admin/devices", None),
    ("post", "/api/v1/admin/devices", {"name": "x"}),
    ("patch", "/api/v1/admin/devices/some-id/settings", {"theme": "minimal-tv"}),
    ("patch", "/api/v1/admin/features/movies", {"enabled": False}),
    ("get", "/api/v1/admin/jellyfin/status", None),
]


@pytest.mark.parametrize("method,path,body", ADMIN_ROUTES)
@pytest.mark.parametrize("headers", [{}, {"X-Admin-Key": ""}, {"X-Admin-Key": "wrong"}, {"X-Admin-Key": "test-admin-secret "}])
def test_admin_routes_reject_bad_keys(client, method, path, body, headers):
    r = getattr(client, method)(path, headers=headers, **({"json": body} if body else {}))
    assert r.status_code == 401


@pytest.mark.parametrize("auth", [None, "", "Bearer", "Bearer ", "Token abc", "Bearer not-a-real-token", "Bearer " + "a" * 300])
@pytest.mark.parametrize("method,path,body", [("get", "/api/v1/device/bootstrap", None), ("post", "/api/v1/device/heartbeat", {"firmware": "1"})])
def test_device_routes_reject_bad_tokens(client, auth, method, path, body):
    headers = {} if auth is None else {"Authorization": auth}
    r = getattr(client, method)(path, headers=headers, **({"json": body} if body else {}))
    assert r.status_code == 401


def test_admin_key_is_not_a_device_token(client):
    r = client.get("/api/v1/device/bootstrap", headers={"Authorization": "Bearer test-admin-secret"})
    assert r.status_code == 401


def test_device_token_is_not_an_admin_key(client, device):
    token = device["headers"]["Authorization"][7:]
    assert client.get("/api/v1/admin/devices", headers={"X-Admin-Key": token}).status_code == 401


def test_token_shown_once_and_stored_hashed(client, device):
    listing = client.get("/api/v1/admin/devices", headers=ADMIN).json()
    token = device["headers"]["Authorization"][7:]
    assert all(token not in str(row) for row in listing)


def test_bootstrap_never_exposes_jellyfin_api_key(client, device):
    body = client.get("/api/v1/device/bootstrap", headers=device["headers"]).json()
    assert set(body["media"]) == {"provider", "configured"}


@pytest.mark.parametrize("patch", [{"theme": "neon"}, {"resolution": "8k"}, {"audio_output": "spdif"}, {"theme": 5}])
def test_settings_validation(client, device, patch):
    r = client.patch(f"/api/v1/admin/devices/{device['id']}/settings", headers=ADMIN, json=patch)
    assert r.status_code == 422


def test_unknown_device_and_feature(client):
    assert client.patch("/api/v1/admin/devices/nope/settings", headers=ADMIN, json={"theme": "minimal-tv"}).status_code == 404
    assert client.patch("/api/v1/admin/features/nope", headers=ADMIN, json={"enabled": True}).status_code == 404


@pytest.mark.parametrize("name", ["", "x" * 121])
def test_device_name_validation(client, name):
    assert client.post("/api/v1/admin/devices", headers=ADMIN, json={"name": name}).status_code == 422


def test_heartbeat_validation(client, device):
    assert client.post("/api/v1/device/heartbeat", headers=device["headers"], json={"firmware": ""}).status_code == 422
