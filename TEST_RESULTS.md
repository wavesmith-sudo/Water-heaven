# Verification — 2026-11-21 (v0.4)

Host: Ubuntu 24.04 container, Python 3.13, Qt 6.4.2 (distro packages), GStreamer 1.24.

| Check | Command | Result |
|---|---|---|
| Shell / Python syntax | `bash scripts/verify.sh` | PASS |
| Backend unit tests | `cd services/backend && pytest` | **48 passed** (1 original + 46 negative-auth/validation + OpenAPI snapshot) |
| Qt app build (desktop Qt 6.4) | `bash scripts/verify.sh --qt` | PASS, no compiler warnings with `-Wall -Wextra` |
| Qt unit tests vs fake Jellyfin | `ctest` in the build dir | **18 passed** |
| QML load + scripted navigation | `CM5_HOME_SMOKE_MS=… cm5-home` (offscreen) | PASS, 0 QML warnings |
| Buildroot Kconfig resolution | `device/cm5-os/scripts/configure.sh` on 2026.02.3 | **46/46** fragment symbols applied (v0.3 fragment: 8/15) |
| Buildroot full image build | `make` | **NOT RUN**: the cloud environment cannot reach source mirrors |
| Docker Compose stack | — | NOT RUN |
| Real Jellyfin server, playback of real media | — | NOT RUN (fake server only; playback path exercised up to the error overlay) |
| CM5-NANO-B hardware, HDMI, remote | — | NOT RUN |

CI (`.github/workflows/ci.yml`) runs the backend job on Python 3.11 and the Qt job on Ubuntu 24.04.
