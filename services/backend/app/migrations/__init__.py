"""Alembic migrations, run automatically at API startup.

Create a new revision after changing app/db.py:
    cd services/backend && alembic revision --autogenerate -m "describe change"
Review the generated file; autogenerate misses some changes (e.g. server defaults).
"""
from pathlib import Path

from alembic import command
from alembic.config import Config
from sqlalchemy import inspect

from ..db import DATABASE_URL, engine

HERE = Path(__file__).resolve().parent
BASELINE = "0001_v0_3_schema"


def alembic_config() -> Config:
    cfg = Config()
    cfg.set_main_option("script_location", str(HERE))
    cfg.set_main_option("sqlalchemy.url", DATABASE_URL.replace("%", "%%"))
    return cfg


def upgrade_database() -> None:
    cfg = alembic_config()
    tables = set(inspect(engine).get_table_names())
    if "devices" in tables and "alembic_version" not in tables:
        # Database created by v0.3's create_all(): it already has the baseline schema.
        command.stamp(cfg, BASELINE)
    command.upgrade(cfg, "head")
