"""Schema as created by the v0.3 prototype's create_all().

Revision ID: 0001_v0_3_schema
"""
from alembic import op
import sqlalchemy as sa

revision = "0001_v0_3_schema"
down_revision = None
branch_labels = None
depends_on = None


def upgrade():
    op.create_table(
        "devices",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("name", sa.String(120), nullable=False),
        sa.Column("token_hash", sa.String(64), nullable=False),
        sa.Column("firmware", sa.String(60), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("last_seen", sa.DateTime(timezone=True), nullable=True),
        sa.Column("settings", sa.JSON(), nullable=False),
    )
    op.create_table(
        "features",
        sa.Column("name", sa.String(80), primary_key=True),
        sa.Column("enabled", sa.Boolean(), nullable=False),
    )


def downgrade():
    op.drop_table("features")
    op.drop_table("devices")
