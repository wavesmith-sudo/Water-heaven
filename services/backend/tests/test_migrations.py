"""Migrations build the same schema as the models, and upgrade a v0.3 database in place."""
import sqlite3
import subprocess
import sys
import textwrap
from pathlib import Path

BACKEND = Path(__file__).resolve().parents[1]

V03_SCHEMA = """
CREATE TABLE devices (id VARCHAR(36) NOT NULL PRIMARY KEY, name VARCHAR(120) NOT NULL,
  token_hash VARCHAR(64) NOT NULL, firmware VARCHAR(60) NOT NULL, created_at DATETIME NOT NULL,
  last_seen DATETIME, settings JSON NOT NULL);
CREATE TABLE features (name VARCHAR(80) NOT NULL PRIMARY KEY, enabled BOOLEAN NOT NULL);
INSERT INTO devices VALUES ('d1', 'Old CM5', '%s', '0.3', '2026-01-01 00:00:00', NULL, '{}');
"""

CHECK = textwrap.dedent("""
    from alembic.autogenerate import compare_metadata
    from alembic.migration import MigrationContext
    from app.db import Base, engine
    from app.migrations import upgrade_database
    upgrade_database()
    with engine.connect() as conn:
        diff = compare_metadata(MigrationContext.configure(conn), Base.metadata)
    print("DIFF", diff)
    assert not diff, diff
""")


def run(db_path):
    env = {"DATABASE_URL": f"sqlite:///{db_path}", "PATH": "/usr/bin:/bin"}
    return subprocess.run([sys.executable, "-c", CHECK], cwd=BACKEND, env=env, capture_output=True, text=True)


def test_fresh_database_matches_models(tmp_path):
    r = run(tmp_path / "fresh.db")
    assert r.returncode == 0, r.stderr + r.stdout


def test_v03_database_upgrades_in_place(tmp_path):
    import hashlib
    db = tmp_path / "v03.db"
    token_hash = hashlib.sha256(b"old-token").hexdigest()
    con = sqlite3.connect(db)
    con.executescript(V03_SCHEMA % token_hash)
    con.close()
    r = run(db)
    assert r.returncode == 0, r.stderr + r.stdout
    con = sqlite3.connect(db)
    assert con.execute("SELECT name, revoked_at FROM devices").fetchall() == [("Old CM5", None)]
    assert con.execute("SELECT version_num FROM alembic_version").fetchone()[0] == "0002_pairing_and_revocation"
    con.close()
