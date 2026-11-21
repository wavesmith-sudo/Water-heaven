#!/usr/bin/env bash
set -euo pipefail
# Usage: ./scripts/configure.sh /path/to/buildroot [base_defconfig]
# Buildroot must be checked out at the release recorded in ../toolchain.lock.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BR="${1:?Pass a Buildroot source directory}"
BASE="${2:-raspberrypicm5io_defconfig}"
EXPECTED="$(sed -n 's/^BUILDROOT_TAG=//p' "$ROOT/toolchain.lock")"
ACTUAL="$(git -C "$BR" describe --tags --exact-match 2>/dev/null || echo unknown)"
if [ "$ACTUAL" != "$EXPECTED" ]; then
  echo "warning: Buildroot is at '$ACTUAL', toolchain.lock pins '$EXPECTED'" >&2
fi
cd "$BR"
# The CM5 IO board defconfig is a STARTING POINT for the CM5-NANO-B, not a qualification.
make BR2_EXTERNAL="$ROOT/br2-external" "$BASE"
python3 "$ROOT/scripts/merge_config.py" "$BR/.config" "$ROOT/br2-external/configs/cm5_media_defconfig"
make BR2_EXTERNAL="$ROOT/br2-external" olddefconfig
python3 "$ROOT/scripts/check_config.py" "$BR/.config" "$ROOT/br2-external/configs/cm5_media_defconfig"
printf 'Configured. Build with: make BR2_EXTERNAL=%q -j$(nproc)\n' "$ROOT/br2-external"
