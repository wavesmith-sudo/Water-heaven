# Verification — 2026-11-21 (v0.4 + device pairing)

Host: Ubuntu 24.04 container, Python 3.13, Qt 6.4.2 (distro packages), GStreamer 1.24.

| Check | Command | Result |
|---|---|---|
| Shell / Python syntax | `bash scripts/verify.sh` | PASS |
| Backend unit tests | `cd services/backend && pytest` | **62 passed** (incl. negative auth, pairing, revocation, rotation, migrations from v0.3 schema, OpenAPI snapshot) |
| Qt app build (desktop Qt 6.4) | `bash scripts/verify.sh --qt` | PASS, no compiler warnings with `-Wall -Wextra` |
| Qt unit tests vs fake Jellyfin | `ctest` in the build dir | **18 passed** |
| Qt CloudClient tests vs the real backend (uvicorn) | `ctest` in the build dir | **9 passed** |
| QML load + scripted navigation | `CM5_HOME_SMOKE_MS=… cm5-home` (offscreen) | PASS, 0 QML warnings |
| Buildroot Kconfig resolution | `device/cm5-os/scripts/configure.sh` on 2026.02.3 | **46/46** fragment symbols applied (v0.3 fragment: 8/15) |
| Buildroot full image build | `.github/workflows/cm5-image.yml` | **PASS** (Water-heaven run 44975416286, commit 81b2466, 2 h 6 min from scratch on a 4-core runner). 1.1 GB `sdcard.img`; checks confirm `/usr/bin/cm5-home`, Qt Quick and Qt Multimedia are in the rootfs. Built from the baseline branch; the cloud pairing code on this branch has not been image-built yet. Not yet booted on hardware |
| Docker Compose stack | — | NOT RUN |
| Real Jellyfin server, playback of real media | — | NOT RUN (fake server only; playback path exercised up to the error overlay) |
| CM5-NANO-B hardware, HDMI, remote | — | NOT RUN |

CI (`.github/workflows/ci.yml`) runs the backend job on Python 3.11 and the Qt job on Ubuntu 24.04.
