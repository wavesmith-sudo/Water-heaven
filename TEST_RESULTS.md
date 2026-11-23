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
| Retro Games logic (Node 22) | `node --test device/cm5-os/app/tests/games_logic.test.mjs` | **11 passed** (lane/ring rules, collisions, scoring, every generated board solvable in par) |
| Qt GameScores test | `ctest` in the build dir | **2 passed** |
| Retro Games scripted play (each game: start, play, pause, quit) | `verify.sh --qt` smoke | PASS, 0 QML warnings |
| QML load + scripted navigation | `CM5_HOME_SMOKE_MS=… cm5-home` (offscreen) | PASS, 0 QML warnings |
| Buildroot Kconfig resolution | `device/cm5-os/scripts/configure.sh` on 2026.02.3 | **48/48** fragment symbols applied (v0.3 fragment: 8/15) |
| Buildroot full image build | `.github/workflows/cm5-image.yml` | **PASS** on both branches in Water-heaven: baseline run 44975416286 (commit 81b2466, 2 h 6 min) and this branch, run 44990508696 (commit ded50cc, 2 h 17 min, includes the cloud pairing client). Each gives a 1.1 GB `sdcard.img`; checks confirm `/usr/bin/cm5-home`, Qt Quick and Qt Multimedia are in the rootfs. The playback branch's image (adds QtConcurrent) has not been confirmed on this combined branch. Not yet booted on hardware |
| Docker Compose stack | — | NOT RUN |
| Real Jellyfin server | — | NOT RUN (fake server only; its PlaybackInfo imitates Jellyfin's decisions in outline) |
| CM5-NANO-B hardware, HDMI, remote | — | NOT RUN |
| Retro Games frame rate on CM5 | — | NOT RUN |

CI (`.github/workflows/ci.yml`) runs the backend job on Python 3.11 and the Qt job on Ubuntu 24.04.
