# Browser preview (Cloudflare Pages)

Runs the device's Retro Arcade QML (`../app/qml`, unchanged) in a web browser with Qt for
WebAssembly, so the UI can be clicked through without a CM5 or a Jellyfin server.

- `DemoJellyfinClient` stands in for `JellyfinClient` with the same QML API and a built-in
  catalogue of made-up movies, shows, episodes and tracks. Sign-in accepts anything. Nothing
  is sent over the network, and no real titles, artwork or credentials are involved.
- `DemoPosterProvider` draws synthwave posters for `image://jellyfin/<id>`.
- Every title plays the same generated 15 s clip (`make_demo_clip.sh`), served next to the page.
- `check_api.py` fails the build if the real client gains a property, method or signal the demo
  client lacks.

The page is public: anyone with the link can open it. It contains only the demo data above.

## Deploy

`.github/workflows/web-preview.yml` builds on pushes that touch the app or this folder and
deploys to the Cloudflare Pages project `retro-arcade-preview` when the repository has the
secrets `CLOUDFLARE_API_TOKEN` (an API token with **Account → Cloudflare Pages → Edit**) and
`CLOUDFLARE_ACCOUNT_ID`. The run summary prints the URL. The production URL follows `main`;
other branches get `<branch>.retro-arcade-preview.pages.dev`.

## Check locally with desktop Qt

```sh
cmake -S device/cm5-os/web-preview -B build-preview && cmake --build build-preview
device/cm5-os/web-preview/make_demo_clip.sh /tmp/clip.mp4
CM5_PREVIEW_CLIP=/tmp/clip.mp4 ./build-preview/cm5-home-preview
# headless: QT_QPA_PLATFORM=offscreen CM5_HOME_SMOKE_MS=800 CM5_HOME_KEYS=Return CM5_HOME_SCREENSHOT=/tmp/s.png
```

## Not covered

The browser build does not exercise the device's real Jellyfin client, GStreamer, HDMI or
remote input. Playback in the browser goes through Qt Multimedia's WebAssembly backend, which
is not what the CM5 uses.
