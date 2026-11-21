# Boot and validation plan

1. Use a Linux build host with ample disk space. Clone a **specific released** Buildroot version and record its commit hash. Check its `configs/raspberrypi5_defconfig` exists; if not, stop and select a compatible release.
2. Run `./scripts/configure.sh /path/to/buildroot` from this project directory.
3. Review Kconfig warnings. The fragment contains version-dependent Qt6 and Bluetooth symbols; inspect `make menuconfig` and adjust if a symbol is absent. Do not assume all selections are active.
4. Run `make BR2_EXTERNAL=/absolute/path/to/br2-external -j$(nproc)` in Buildroot.
5. Inspect `output/images/` and the base board's `board/raspberrypi/` image-generation instructions. Do not flash an image until boot firmware, DTB, CM5 pin configuration, and carrier compatibility are confirmed.
6. Connect the CM5-NANO-B to HDMI, a known-good rated 5V supply, and a USB keyboard for initial navigation.
7. Validate kernel boot, HDMI/KMS, Qt EGLFS startup, keyboard arrows/Enter, network, and Bluetooth separately.
8. Collect `/var/log/cm5-home.log`, `dmesg`, `vcgencmd` (if present), and `ls /dev/dri` for debugging.

**Important:** The starter's Qt Quick UI uses software scene-graph rendering (`QT_QUICK_BACKEND=software`) as a compatibility-first experiment. EGLFS/DRM setup, Qt plugin availability, and CM5 GPU acceleration require target validation. If software Qt Quick fails, test the target Qt graphics backend and change the launcher. No Jellyfin playback or remote HID pairing is implemented yet.
