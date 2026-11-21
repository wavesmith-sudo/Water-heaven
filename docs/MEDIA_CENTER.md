# Media Center v0.3 — implementation and limitations

## Implemented source
- Qt 6 Quick Retro Arcade home navigation, Movies / TV Shows / Music / Continue Watching library requests.
- Jellyfin username/password login using `/Users/AuthenticateByName`, token persisted via QSettings (prototype only).
- Jellyfin item listing and primary artwork retrieval, including a first-pass direct stream URL.
- Qt Multimedia `MediaPlayer` and `VideoOutput` integration for playable video/audio files.
- Buildroot Qt Network and Qt Multimedia package dependency declarations.
- Settings page for server URL and login.

## Critical incomplete items
- **Not built or run** against a real Buildroot toolchain or CM5. Qt Multimedia backend and hardware acceleration require validation.
- Direct-play URL does not negotiate Jellyfin PlaybackInfo, codecs, subtitles, or transcoding. Some files will fail.
- TV series selection currently opens a placeholder; episode browsing not implemented.
- Playback progress reporting, resume seek, favorites, music-focused UI, profiles, search, and device pairing are not implemented.
- Remote HID event mapping beyond keyboard-equivalent D-pad / OK / Back is unimplemented.
- QSettings token persistence is not secure credential storage. Replace with protected device credential service.
- The prototype image URL embeds an API token in its query string, which can leak in logs; replace with authenticated image fetching.
- No pinning of Buildroot source revision; use a verified release and lock dependencies.
- QML Settings TextFields need focus-handling verification on a real remote; use a TV on-screen keyboard.
- The app currently authenticates directly to Jellyfin; future architecture should use the platform's device authorization gateway.

## Build prerequisites
Qt 6 Core, Gui, Quick, Qml, Network, Multimedia; a working multimedia backend and SSL/TLS support. On Buildroot, merge `cm5_media_defconfig` into the supported Raspberry Pi 5 base configuration, check package symbols with `make menuconfig`, and build. No bootable image is included.

## Integration tests to run
1. Configure a test Jellyfin server with HTTPS and a test user.
2. Sign in via Settings and verify token and user ID retrieval.
3. Browse Movie, Series and Audio libraries; verify artwork and navigation.
4. Attempt H.264/AAC MP4 direct playback; record audio/video sync and CPU load.
5. Test unsupported codec to document fallback need.
6. Validate Wi-Fi reconnect, Bluetooth keyboard events, and long-duration playback.
7. Audit tokens, TLS verification, and server URL validation before production.
