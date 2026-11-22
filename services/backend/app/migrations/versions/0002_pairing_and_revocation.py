"""Device pairing requests and token revocation.

Revision ID: 0002_pairing_and_revocation
"""
from alembic import op
import sqlalchemy as sa

revision = "0002_pairing_and_revocation"
down_revision = "0001_v0_3_schema"
branch_labels = None
depends_on = None


def upgrade():
    with op.batch_alter_table("devices") as batch:
        batch.add_column(sa.Column("revoked_at", sa.DateTime(timezone=True), nullable=True))
        batch.create_index("ix_devices_token_hash", ["token_hash"])
    op.create_table(
        "pairing_requests",
        sa.Column("id", sa.String(36), primary_key=True),
        sa.Column("user_code_hash", sa.String(64), nullable=False),
        sa.Column("device_code_hash", sa.String(64), nullable=False),
        sa.Column("device_name", sa.String(120), nullable=False),
        sa.Column("firmware", sa.String(60), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("expires_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("approved_device_id", sa.String(36), sa.ForeignKey("devices.id"), nullable=True),
        sa.Column("claimed_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index("ix_pairing_requests_user_code_hash", "pairing_requests", ["user_code_hash"], unique=True)
    op.create_index("ix_pairing_requests_device_code_hash", "pairing_requests", ["device_code_hash"], unique=True)


def downgrade():
    op.drop_table("pairing_requests")
    with op.batch_alter_table("devices") as batch:
        batch.drop_index("ix_devices_token_hash")
        batch.drop_column("revoked_at")
