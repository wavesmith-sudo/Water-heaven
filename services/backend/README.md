# Retro Arcade Cloud Backend v0.1

Working **prototype starter** for the CM5 Buildroot media dongle: device registration, per-device settings, global feature flags, heartbeat, Jellyfin connectivity status, and CM5 bootstrap API.

## Stack
FastAPI, SQLAlchemy, PostgreSQL, Docker Compose, Jellyfin. Python reference client for the device. No web admin UI yet; FastAPI `/docs` is available locally.

## Start locally with Docker
1. Copy `.env.example` to `.env`.
2. Generate secrets, e.g. `openssl rand -hex 32` for `ADMIN_API_KEY` and `POSTGRES_PASSWORD`. Add `POSTGRES_PASSWORD` to `.env`; update `DATABASE_URL` only if running outside Docker. Avoid special URI characters in the password or URL-encode them.
3. Run `mkdir -p media && docker compose up --build -d`.
4. Check `curl http://127.0.0.1:8000/health` and browse `http://127.0.0.1:8000/docs` on the host.
5. Access Jellyfin setup at `http://127.0.0.1:8096` on the host. Add your own authorized content to `media/` and configure a Jellyfin API key if needed.

### Pair a device (preferred)
On the TV: Settings → Arcade Cloud → enter the API URL → Pair. It shows a code like `XG83-7WD4`. Approve it:
```
curl -X POST http://127.0.0.1:8000/api/v1/admin/pairing/XG83-7WD4/approve \
  -H "X-Admin-Key: YOUR_ADMIN_API_KEY" -H 'Content-Type: application/json' -d '{"name":"Living Room"}'
```
The TV picks up its own token within a few seconds. Revoke with `POST /api/v1/admin/devices/{id}/revoke`.

### Database migrations
Alembic migrations in `app/migrations` run automatically at startup. A database created by the
v0.1 prototype is detected and upgraded in place. After changing `app/db.py`, add a revision with
`alembic revision --autogenerate -m "..."` and review it.

### Register a device manually (fallback)
```
curl -X POST http://127.0.0.1:8000/api/v1/admin/devices \
  -H "X-Admin-Key: YOUR_ADMIN_API_KEY" -H 'Content-Type: application/json' \
  -d '{"name":"Living Room CM5"}'
```
The response includes a **one-time device token**. Do not embed administrator keys in firmware. Provision device tokens securely.

### Fetch CM5 bootstrap configuration
```
ARCADE_API_URL=http://127.0.0.1:8000 ARCADE_DEVICE_TOKEN=YOUR_TOKEN python device-client/bootstrap.py
```
The API returns device name, theme, feature flags and whether Jellyfin credentials have been configured.

### Update a feature
```
curl -X PATCH http://127.0.0.1:8000/api/v1/admin/features/retro_games \
  -H 'X-Admin-Key: YOUR_ADMIN_API_KEY' -H 'Content-Type: application/json' \
  -d '{"enabled":true}'
```

## Local Python test
```
python -m venv .venv && . .venv/bin/activate
pip install -r requirements-dev.txt
pytest
python scripts/export_openapi.py   # after API changes; commit docs/openapi.json
```

## Security and limitations
- **Prototype only**. The API binds to loopback in Docker Compose; for remote devices, deploy behind a TLS reverse proxy with access controls, rate limiting and firewall rules. Never expose port 8000 directly to the public internet.
- Admin uses a shared secret. Replace with identity provider, role-based access and audit logs before production.
- Device tokens are randomly generated and stored only as SHA-256 hashes. Add revocation, rotation, scoped credentials, secure manufacturing provisioning and account/device ownership before shipping.
- Settings are validated but currently managed only by administrators. No end-user login, pairing UI, or full fleet dashboard.
- Jellyfin connectivity status is implemented; **library catalog, authentication delegation and playback are not yet implemented**. Do not give shared Jellyfin administrator API keys to devices.
- Retro games are a disabled-by-default feature flag; ROM distribution, content licensing, downloads and emulator integration are not implemented.
- The Jellyfin container is exposed on host loopback only. For cloud streaming, configure HTTPS and authenticated access separately.
- Use only media and game content you are authorized to host and distribute.

## Proposed next phases
1. Device pairing flow and per-user identities.
2. Jellyfin adapter for metadata and per-user authorized playback.
3. Qt/QML Retrofit Arcade UI consuming the bootstrap API.
4. Admin dashboard and device inventory.
5. Signed OTA updates and rollback support.
