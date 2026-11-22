# Verification — 2026-11-22 (v0.4 + device pairing + playback negotiation)

Host: Ubuntu 24.04 container, Python 3.13, Qt 6.4.2 (distro packages), GStreamer 1.24.

| Check | Command | Result |
|---|---|---|
| Shell / Python syntax | `bash scripts/verify.sh` | PASS |
| Backend unit tests | `cd services/backend && pytest` | **62 passed** (incl. negative auth, pairing, revocation, rotation, migrations from v0.3 schema, OpenAPI snapshot) |
| Qt app build (desktop Qt 6.4) | `bash scripts/verify.sh --qt` | PASS, no compiler warnings with `-Wall -Wextra` |
| Qt unit tests vs fake Jellyfin | `ctest` in the build dir | **32 passed** (incl. 10 profile decisions, forced transcode, play-session reports) |
| Real playback through `PlayerView.qml` (desktop GStreamer 1.24) | `tst_playerview` | **3 passed**: direct-play MP4, HLS transcode, corrupt direct stream falling back to HLS |
| Qt CloudClient tests vs the real backend (uvicorn) | `ctest` in the build dir | **9 passed** |
| QML load + scripted navigation | `CM5_HOME_SMOKE_MS=… cm5-home` (offscreen) | PASS, 0 QML warnings |
| Buildroot Kconfig resolution | `device/cm5-os/scripts/configure.sh` on 2026.02.3 | **48/48** fragment symbols applied (v0.3 fragment: 8/15) |
| Buildroot full image build | `.github/workflows/cm5-image.yml` | First run reached qt6multimedia (~3 h) and found a missing QtConcurrent dependency, now fixed; re-running |
| Docker Compose stack | — | NOT RUN |
| Real Jellyfin server | — | NOT RUN (fake server only; its PlaybackInfo imitates Jellyfin's decisions in outline) |
| CM5-NANO-B hardware, HDMI, remote | — | NOT RUN |

CI (`.github/workflows/ci.yml`) runs the backend job on Python 3.11 and the Qt job on Ubuntu 24.04.
