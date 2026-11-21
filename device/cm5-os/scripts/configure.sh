#!/usr/bin/env bash
set -euo pipefail
# Usage: ./scripts/configure.sh /path/to/buildroot
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BR="${1:?Pass a Buildroot source directory}"
cd "$BR"
# Raspberry Pi 5 board support is used as a STARTING POINT, not CM5 certification.
make BR2_EXTERNAL="$ROOT/br2-external" raspberrypi5_defconfig
# Merge selected external options using Buildroot's Kconfig interface.
# BR2_EXTERNAL must be supplied again to each make invocation.
python3 "$ROOT/scripts/merge_config.py" "$BR/.config" "$ROOT/br2-external/configs/cm5_media_defconfig"
make BR2_EXTERNAL="$ROOT/br2-external" olddefconfig
printf 'Configured. Build with: make BR2_EXTERNAL=%q -j$(nproc)\n' "$ROOT/br2-external"
