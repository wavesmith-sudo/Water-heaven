# Verification — 2026-11-21 (v0.4 + device pairing)

Host: Ubuntu 24.04 container, Python 3.13, Qt 6.4.2 (distro packages), GStreamer 1.24.

| Check | Command | Result |
|---|---|---|
| Shell / Python syntax | `bash scripts/verify.sh` | PASS |
| Backend unit tests | `cd services/backend && pytest` | **62 passed** (incl. negative auth, pairing, revocation, rotation, migrations from v0.3 schema, OpenAPI snapshot) |
| Qt app build (desktop Qt 6.4) | `bash scripts/verify.sh --qt` | PASS, no compiler warnings with `-Wall -Wextra` |
| Qt unit tests vs fake Jellyfin | `ctest` in the build dir | **18 passed** |
| Qt CloudClient tests vs the real backend (uvicorn) | `ctest` in the build dir | **9 passed** |
| Retro Games logic (Node 22) | `node --test device/cm5-os/app/tests/games_logic.test.mjs` | **11 passed** (lane/ring rules, collisions, scoring, every generated board solvable in par) |
| Qt GameScores test | `ctest` in the build dir | **2 passed** |
| Retro Games scripted play (each game: start, play, pause, quit) | `verify.sh --qt` smoke | PASS, 0 QML warnings |
| QML load + scripted navigation | `CM5_HOME_SMOKE_MS=… cm5-home` (offscreen) | PASS, 0 QML warnings |
| Buildroot Kconfig resolution | `device/cm5-os/scripts/configure.sh` on 2026.02.3 | **46/46** fragment symbols applied (v0.3 fragment: 8/15) |
| Buildroot full image build | `.github/workflows/cm5-image.yml` | First run reached qt6multimedia (~3 h) and found a missing QtConcurrent dependency, now fixed; re-running |
| Docker Compose stack | — | NOT RUN |
| Real Jellyfin server, playback of real media | — | NOT RUN (fake server only; playback path exercised up to the error overlay) |
| CM5-NANO-B hardware, HDMI, remote | — | NOT RUN |
| Retro Games frame rate on CM5 | — | NOT RUN |

CI (`.github/workflows/ci.yml`) runs the backend job on Python 3.11 and the Qt job on Ubuntu 24.04.
