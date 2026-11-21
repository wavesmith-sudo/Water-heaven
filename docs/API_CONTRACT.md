# Current v0.2 API contract (prototype)

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
| POST | `/api/v1/device/token/rotate` | `Authorization: Bearer <device_token>` | issue a new token, invalidate the current one |
| POST | `/api/v1/pairing/start` | none (rate limited per client) | device requests a user code + secret device code |
| POST | `/api/v1/pairing/poll` | device code in body | 202 pending, 200 with device token (once), 410 expired, 404 unknown/used |
| GET | `/api/v1/admin/pairing` | `X-Admin-Key` | list pending pairing requests (no codes) |
| POST | `/api/v1/admin/pairing/{user_code}/approve` | `X-Admin-Key` | approve a code shown on a TV; creates the device |
| POST | `/api/v1/admin/devices/{id}/revoke` | `X-Admin-Key` | permanently disable a device token |

## Pairing flow
Modelled on the OAuth 2.0 device authorization grant (RFC 8628). The TV calls `pairing/start` and
shows the 8-character user code (`XXXX-XXXX`, no look-alike characters). The owner approves it; the
TV polls `pairing/poll` every `interval` seconds and receives its device token once. Codes expire
after 10 minutes. User codes and device codes are stored only as SHA-256 hashes, and the device
token is minted at hand-over, so no usable secret is ever stored in plain text.

Approval is admin-only until user accounts exist; a web dashboard will call the same endpoint.

**Not implemented:** user login and per-user ownership of devices, media catalog, playback session/URL, games API, OTA. Do not expose a Jellyfin administrator API key in device bootstrap. Introduce API schema validation and per-user authorization before content endpoints.
