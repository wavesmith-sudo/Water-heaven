#!/usr/bin/env bash
# Source-level checks that run on any Linux host (and in CI).
# Usage: scripts/verify.sh            backend + scripts
#        scripts/verify.sh --qt       also build and test the Qt app (needs Qt 6.4+ dev packages)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WITH_QT=0
[ "${1:-}" = "--qt" ] && WITH_QT=1

echo '== Shell syntax =='
bash -n "$ROOT/device/cm5-os/scripts/configure.sh"
bash -n "$ROOT/scripts/smoke_api.sh"
bash -n "$ROOT/device/cm5-os/tools/make_test_media.sh"
sh -n "$ROOT/device/cm5-os/br2-external/board/cm5/overlay/etc/init.d/S95cm5home"

echo '== Python compilation =='
python3 -m compileall -q "$ROOT/services/backend/app" "$ROOT/services/backend/device-client" \
  "$ROOT/services/backend/scripts" "$ROOT/device/cm5-os/scripts" "$ROOT/device/cm5-os/tools"

echo '== Backend unit tests =='
(cd "$ROOT/services/backend" && python3 -m pytest)

echo '== Retro Games logic tests =='
if command -v node >/dev/null 2>&1; then
  node --test "$ROOT/device/cm5-os/app/tests/games_logic.test.mjs"
else
  echo 'node not found; skipping (install Node.js 18+ to run them)'
fi

if [ "$WITH_QT" = 1 ]; then
  BUILD="${CM5_HOME_BUILD_DIR:-$ROOT/build/cm5-home-host}"
  echo "== Qt app: configure and build ($BUILD) =="
  cmake -S "$ROOT/device/cm5-os/app" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCM5_HOME_BUILD_TESTS=ON
  cmake --build "$BUILD" -j"$(nproc)"
  echo '== Qt app: unit tests =='
  (cd "$BUILD" && QT_QPA_PLATFORM=offscreen ctest --output-on-failure)
  echo '== Qt app: QML load smoke test (offscreen) =='
  SMOKE_HOME="$(mktemp -d)"
  XDG_CONFIG_HOME="$SMOKE_HOME" XDG_RUNTIME_DIR="$SMOKE_HOME" QT_QPA_PLATFORM=offscreen \
    CM5_HOME_SIZE=1920x1080 CM5_HOME_SMOKE_MS=1500 CM5_HOME_KEYS="Right,Return,Escape,Right,Right,Right,Right,Right,Return,Escape" \
    "$BUILD/cm5-home"
  echo '== Qt app: Retro Games smoke test (play each game, pause, quit) =='
  XDG_CONFIG_HOME="$SMOKE_HOME" XDG_RUNTIME_DIR="$SMOKE_HOME" QT_QPA_PLATFORM=offscreen \
    CM5_HOME_SIZE=1280x720 CM5_HOME_SMOKE_MS=1500 \
    CM5_HOME_KEYS="Right,Right,Right,Return,Return,Left,Return,Wait,Escape,Escape,Right,Return,Right,Return,Wait,Escape,Return,Escape,Escape,Right,Return,Return,Down,Return,Escape,Escape,Escape" \
    "$BUILD/cm5-home"
fi

echo '== Source checks passed; Buildroot image and CM5 hardware were NOT tested here =='
