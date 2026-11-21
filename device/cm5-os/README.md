# CM5 Media OS — Buildroot Starter v0.4

A **source starter**, not a prebuilt or hardware-validated firmware image, for a Raspberry Pi Compute Module 5 on the Waveshare CM5-NANO-B carrier.

## Included
- Buildroot br2-external tree and CM5 home-screen package
- Qt 6 Quick Retro Arcade home screen with Jellyfin browsing and playback (`app/`)
- Host unit tests and a fake Jellyfin server for development (`app/tests/`, `tools/`)
- Pinned Buildroot release (`toolchain.lock`) and a Kconfig check (`scripts/check_config.py`)
- SysV init launch script and root filesystem overlay
- Config fragment for Qt, Wi-Fi and Bluetooth tools
- Build/configuration script and boot validation checklist

## Prerequisites
Linux development host; Buildroot at the tag in `toolchain.lock` (base `raspberrypicm5io_defconfig`); host build dependencies; 30+ GB disk; CM5 + carrier + HDMI + power.

## Configure / build
```sh
./scripts/configure.sh /absolute/path/to/buildroot
cd /absolute/path/to/buildroot
make BR2_EXTERNAL=/absolute/path/to/cm5-media-os-starter/br2-external menuconfig
make BR2_EXTERNAL=/absolute/path/to/cm5-media-os-starter/br2-external -j$(nproc)
```

## Boot sequence (intended)
Pi firmware/EEPROM -> bootloader/DTB -> Linux kernel -> Buildroot init -> S95cm5home -> Qt Quick UI.

## Current limitations
- No verified CM5-NANO-B boot image, device-tree overlay, connector setup, or GPU/video pipeline.
- Jellyfin client is direct-play only (see `docs/MEDIA_CENTER.md`); no RetroArch, Wi-Fi onboarding, pairing UI, update mechanism, or security hardening.
- The fragment resolves cleanly in Kconfig, but no image has been compiled yet.
- Cloud Jellyfin should use HTTPS, per-user credentials and device authorization; never embed API secrets in firmware.
- Verify the board power requirements before booting; do not infer USB-C power capabilities from the enclosure drawing.

See `docs/BOOT_AND_TEST.md` for bring-up steps.
