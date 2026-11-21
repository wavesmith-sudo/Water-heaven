# Retro Arcade CM5 Platform — Claude Engineering Handoff v0.3

Read `AUDIT.md`, `docs/MEDIA_CENTER.md`, `docs/IMPLEMENTATION_PLAN.md`, and `docs/SECURITY.md` first.

## Current delivery
This repository contains the Buildroot OS starter, a **source-level Qt 6 Retro Arcade media center** with Jellyfin authentication/library calls and Qt Multimedia playback attempt, a FastAPI cloud backend, hardware enclosure references, and verification scripts. **No full CM5 OS image has been built or hardware tested.** Do not claim production readiness.

## Prioritized tasks
1. Reproduce and fix Qt6 Buildroot compilation against a pinned supported Buildroot version; verify the Qt Multimedia backend.
2. Fix the Jellyfin client (secure token handling, authenticated artwork requests, PlaybackInfo negotiation, progress sync, episode browsing).
3. Implement end-to-end device pairing to the cloud backend; remove hardcoded prototype device identifiers.
4. Test HDMI audio/video and remote input on the CM5-NANO-B.
5. Implement dashboard, RetroArch integration, secure OTA and tests.

Preserve the selected Retro Arcade aesthetic, CM5-NANO-B hardware, and cloud Jellyfin backend. Report tested versus untested features accurately.
