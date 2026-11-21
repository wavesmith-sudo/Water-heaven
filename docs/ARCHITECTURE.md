# Architecture and boot path

```text
TV + Bluetooth hybrid remote
          |
    CM5 / Buildroot
    |  Qt Quick Retro Arcade UI
    |  local device agent (to implement)
    |  GStreamer/player pipeline (to implement)
    |  RetroArch local games (future)
    |               \
    | HTTPS control  \ direct media HTTPS
    v                 v
 FastAPI cloud API    Jellyfin media server
    |                 |
 PostgreSQL          media storage
```

Boot sequence: CM5 firmware -> Linux kernel/DTB -> Buildroot init -> network/Bluetooth services -> device agent -> Qt QML home UI -> authenticated bootstrap -> Jellyfin library browsing. Offline startup must remain usable for settings and cached games. The present source only implements a small subset.

The cloud API manages device identity, configuration, features and library metadata. **Video bytes should not be proxied through the control API** except when a documented security/network constraint requires it.

The custom remote is Bluetooth HID; the OS translates key events into navigation/media actions. Keep controller mappings separate from UI theme.
