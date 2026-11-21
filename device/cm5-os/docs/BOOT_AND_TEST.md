# Boot and validation plan

## Pinned inputs
`device/cm5-os/toolchain.lock` pins Buildroot **2026.02.3** (LTS, Qt 6.9.1) and the base
`raspberrypicm5io_defconfig`. The CM5 IO board config is the closest upstream starting point; the
Waveshare CM5-NANO-B carrier is not upstream and still needs its own validation.

## Configure (verified)
```sh
git clone --branch 2026.02.3 https://gitlab.com/buildroot.org/buildroot.git
./scripts/configure.sh /path/to/buildroot
```
`configure.sh` applies the base defconfig, merges `br2-external/configs/cm5_media_defconfig`, runs
`olddefconfig`, then `scripts/check_config.py` fails if Kconfig dropped any requested symbol.

Result on 2026-11-21: **45/45 fragment symbols applied**. For comparison, the v0.3 fragment on
`raspberrypi5_defconfig` applied 8/15: it never enabled the `BR2_PACKAGE_QT6` menu, so Qt, Qt Quick,
EGLFS, Qt Multimedia and the `cm5-home` package were all silently dropped, and EGLFS had no
EGL/GBM provider. The v0.4 fragment adds Mesa (V3D + VC4), Qt Quick, OpenSSL, fonts, and a
GStreamer + ALSA backend for Qt Multimedia.

## Build (not yet run)
```sh
cd /path/to/buildroot
make BR2_EXTERNAL=/absolute/path/to/device/cm5-os/br2-external -j$(nproc)
```
Expect several hours on a 4-core host and 30+ GB of disk. Inspect `output/images/` and
`board/raspberrypicm5io/` image generation. The cloud environment used for v0.4 cannot reach
upstream source mirrors, so the full build has not been attempted.

## Bring-up on the CM5-NANO-B
1. Do not flash until boot firmware, DTB, CM5 pin configuration and carrier compatibility are confirmed.
2. Connect HDMI, a known-good rated 5 V supply, and a USB keyboard for first navigation.
3. Validate kernel boot, HDMI/KMS (`ls /dev/dri`), Qt EGLFS startup, arrows/Enter, network and Bluetooth separately.
4. If GPU rendering fails, set `QT_QUICK_BACKEND=software` in `/etc/default/cm5-home` and restart
   `/etc/init.d/S95cm5home restart`; report which one worked.
5. Collect `/var/log/cm5-home.log` (previous run in `.1`), `dmesg`, `vcgencmd` if present.

## Known gaps
- HEVC hardware decode needs GStreamer `v4l2codecs`, which needs udev; the base config uses mdev.
- No Wi-Fi onboarding UI, remote pairing UI, update mechanism or security hardening yet.
