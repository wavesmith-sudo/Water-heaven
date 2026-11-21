# Claude delivery sequence

## Milestone 0 — Baseline (first PR)
Run `scripts/verify.sh`, fix test imports, pin Python deps, add API OpenAPI schema snapshot, document results. No feature additions until baseline is reproducible.

## Milestone 1 — CM5 hardware bring-up
Pin Buildroot and kernel/firmware; verify boot media, DRM/KMS, Qt rendering, Wi-Fi and Bluetooth on CM5-NANO-B. Capture logs and build artifacts.

## Milestone 2 — Cloud security foundation
Introduce migrations, per-device provisioning/pairing, token revocation and scoped user identity. Add HTTPS reverse proxy and negative-auth tests.

## Milestone 3 — Media end-to-end
Jellyfin user auth, catalogs, playable URL selection, hardware decode and QML library screens; test direct play on TV.

## Milestone 4 — Retro Arcade and remote
Implement selected neon theme, focus animations and HID mappings, media keys and side L/R. Ensure 1080p and 4K layout scaling.

## Milestone 5 — Fleet and games
RBAC web admin, local legal-game cache/RetroArch, signed OTA and rollback, telemetry and support docs.

For every milestone: list changed files, commands run, test output, untested hardware assumptions, security impact and next blockers.
