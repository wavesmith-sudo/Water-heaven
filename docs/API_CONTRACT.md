# Current v0.1 API contract (prototype)

The machine-readable schema is `docs/openapi.json`; `tests/test_openapi_snapshot.py` fails when the API changes without updating it.

Base path `/api/v1`; API served over HTTPS in any remote deployment.

| Method | Path | Authentication | Purpose |
|---|---|---|---|
| GET | `/health` | none | liveness |
| POST | `/api/v1/admin/devices` | `X-Admin-Key` | register device, return one-time token |
| GET | `/api/v1/admin/devices` | `X-Admin-Key` | list devices |
| PATCH | `/api/v1/admin/devices/{id}/settings` | `X-Admin-Key` | update device settings |
| PATCH | `/api/v1/admin/features/{name}` | `X-Admin-Key` | set global feature flag |
| GET | `/api/v1/admin/jellyfin/status` | `X-Admin-Key` | check configured/reachable |
| GET | `/api/v1/device/bootstrap` | `Authorization: Bearer <device_token>` | retrieve settings and features |
| POST | `/api/v1/device/heartbeat` | `Authorization: Bearer <device_token>` | report firmware |

**Not implemented:** user login, pairing-code flow, token revocation, media catalog, playback session/URL, games API, OTA. Do not expose a Jellyfin administrator API key in device bootstrap. Introduce API schema validation and per-user authorization before content endpoints.
