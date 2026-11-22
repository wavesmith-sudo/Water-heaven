"""Fails when the API surface changes without updating docs/openapi.json.

Regenerate with: python scripts/export_openapi.py   (from services/backend)
"""
import json
from pathlib import Path

from app.main import app

SNAPSHOT = Path(__file__).resolve().parents[3] / "docs" / "openapi.json"


def test_openapi_matches_snapshot():
    assert SNAPSHOT.exists(), "Run services/backend/scripts/export_openapi.py"
    assert json.loads(SNAPSHOT.read_text()) == app.openapi(), (
        "API schema changed. Review, then run services/backend/scripts/export_openapi.py and commit docs/openapi.json"
    )
