# Milestone 0 report — baseline (2026-11-21)

Format per `docs/IMPLEMENTATION_PLAN.md`. This PR also starts Milestones 1, 3 and 4 where the work
could be verified without hardware.

## Changed
- **Baseline**: pinned backend deps (`requirements.txt`, `requirements-dev.txt`); `pytest.ini` +
  `conftest.py` so tests run from a fresh temp DB without import hacks; OpenAPI snapshot
  (`docs/openapi.json`) with a test; negative-auth and validation tests; CI with backend and Qt jobs.
- **Buildroot (M1, config only)**: pinned 2026.02.3 LTS in `toolchain.lock`; base switched to
  `raspberrypicm5io_defconfig`; fragment rewritten (Qt menu, Qt Quick, Mesa V3D/VC4, OpenSSL, fonts,
  GStreamer/ALSA backend); `check_config.py` catches symbols Kconfig drops; `merge_config.py` handles
  `is not set`; init script reads `/etc/default/cm5-home` and defaults to GPU EGLFS/KMS.
- **Jellyfin client (M3)**: header auth, token-free artwork provider, per-install device id, episode
  browsing, audio stream path, resume + progress reporting, sign out, 401 handling, URL validation.
- **UI (M4)**: Retro Arcade theme per concept 2, 1080p-relative scaling, D-pad-navigable Settings,
  lazy player.
- **Dev tooling**: `tools/fake_jellyfin.py`, smoke/screenshot hooks in `main.cpp`, Qt unit tests.

## Commands run
See `TEST_RESULTS.md`.

## Untested hardware assumptions
- The CM5 IO board defconfig/DTB boots the CM5-NANO-B (carrier differences unknown).
- Mesa V3D/VC4 + Qt EGLFS KMS render on the CM5 HDMI output; software fallback is configurable.
- GStreamer software decode of H.264 1080p is fast enough on the CM5's Cortex-A76 cores.
- Qt Quick Controls (Basic style) are installed by Buildroot's `qt6declarative` with Quick enabled.

## Security impact
- Tokens no longer appear in artwork URLs; requests use the Authorization header.
- The stream URL still includes the token (Qt Multimedia limitation); documented in `docs/MEDIA_CENTER.md`.
- Device id is now unique per install; tokens are dropped when the server URL changes.
- Backend behaviour is unchanged; new tests pin down the 401/422 behaviour.

## Next blockers
1. A full Buildroot build on a machine with mirror access (several hours on 4 cores).
2. A real Jellyfin test server and sample media for playback testing.
3. CM5-NANO-B hardware for bring-up.
