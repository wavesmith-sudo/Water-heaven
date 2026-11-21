# CM5 Media OS — Buildroot Starter v0.1

A **source starter**, not a prebuilt or hardware-validated firmware image, for a Raspberry Pi Compute Module 5 on the Waveshare CM5-NANO-B carrier.

## Included
- Buildroot br2-external tree and CM5 home-screen package
- Qt 6 Quick full-screen, keyboard/remote-style navigation demo
- SysV init launch script and root filesystem overlay
- Config fragment for Qt, Wi-Fi and Bluetooth tools
- Build/configuration script and boot validation checklist

## Prerequisites
Linux development host; Buildroot source with `raspberrypi5_defconfig`; appropriate host build dependencies; sufficient disk and RAM; CM5 + carrier + HDMI + power. Use a pinned Buildroot release and record its version.

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
- No Jellyfin client, player, RetroArch, Wi-Fi onboarding, pairing UI, update mechanism, or security hardening.
- Qt 6 Buildroot symbols and graphics plugins may vary by release; check Kconfig and compile logs.
- Cloud Jellyfin should use HTTPS, per-user credentials and device authorization; never embed API secrets in firmware.
- Verify the board power requirements before booting; do not infer USB-C power capabilities from the enclosure drawing.

See `docs/BOOT_AND_TEST.md` for bring-up steps.
