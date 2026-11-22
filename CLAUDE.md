# Retro Arcade CM5 Platform — Engineering notes (v0.4)

Read `AUDIT.md`, `docs/MEDIA_CENTER.md`, `docs/IMPLEMENTATION_PLAN.md`, and `docs/SECURITY.md` first.

## Current delivery
This repository contains the Buildroot OS starter, a Qt 6 Retro Arcade media center (Jellyfin login, library and episode browsing, direct play with progress sync), a FastAPI cloud backend, hardware enclosure references, and verification scripts. The Qt app builds and passes unit and QML smoke tests on desktop Qt 6.4; the Buildroot fragment resolves against the pinned release. The full CM5 SD card image builds on GitHub Actions (`.github/workflows/cm5-image.yml`). **It has not been booted or tested on hardware.** Do not claim production readiness.

Run `bash scripts/verify.sh --qt` before pushing. If the backend API changes, regenerate `docs/openapi.json` with `services/backend/scripts/export_openapi.py`.

## Prioritized tasks
1. Boot the built image on the CM5-NANO-B and verify the Qt Multimedia GStreamer backend on target.
2. Jellyfin client: PlaybackInfo negotiation/transcoding, removing the token from the stream URL, secure token storage, seasons view. (Header auth, authenticated artwork, progress sync and episode browsing are done.)
3. Implement end-to-end device pairing to the cloud backend (pairing code, revocation); feed the cloud-issued device id into the Jellyfin client.
4. Test HDMI audio/video and remote input on the CM5-NANO-B.
5. Implement dashboard, RetroArch integration, secure OTA and tests.

Preserve the selected Retro Arcade aesthetic, CM5-NANO-B hardware, and cloud Jellyfin backend. Report tested versus untested features accurately.
