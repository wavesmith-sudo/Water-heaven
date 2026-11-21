#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo '== Shell syntax =='
bash -n "$ROOT/device/cm5-os/scripts/configure.sh"
bash -n "$ROOT/scripts/smoke_api.sh"
echo '== Python compilation =='
python3 -m compileall -q "$ROOT/services/backend/app" "$ROOT/services/backend/device-client" "$ROOT/device/cm5-os/scripts"
echo '== Backend unit tests =='
(cd "$ROOT/services/backend" && python3 -m pytest -q)
echo '== Source checks passed; Docker, Buildroot and hardware were NOT tested =='
