"""Test configuration.

app.main reads its settings and creates the engine at import time, so the
environment must be set before any test module imports it. Each test session
uses a fresh SQLite file in a temporary directory instead of the working tree.
"""
import os
import tempfile

_tmp = tempfile.mkdtemp(prefix="arcade-test-")
os.environ["DATABASE_URL"] = f"sqlite:///{_tmp}/test_arcade.db"
os.environ["ADMIN_API_KEY"] = "test-admin-secret"
os.environ["JELLYFIN_API_KEY"] = ""

import pytest  # noqa: E402
from fastapi.testclient import TestClient  # noqa: E402

from app.main import app  # noqa: E402

ADMIN = {"X-Admin-Key": "test-admin-secret"}


@pytest.fixture()
def client():
    with TestClient(app) as c:
        yield c


@pytest.fixture()
def device(client):
    r = client.post("/api/v1/admin/devices", headers=ADMIN, json={"name": "Fixture CM5"})
    assert r.status_code == 201
    body = r.json()
    return {"id": body["id"], "headers": {"Authorization": "Bearer " + body["device_token"]}}
