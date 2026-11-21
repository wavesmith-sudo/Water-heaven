# Build and release runbook

## Development workstation
- Linux host, Python 3.11+, Git, compiler/toolchain prerequisites, Docker + Compose.
- Choose and record a specific Buildroot release tag/commit in a future `toolchain.lock` after checking Raspberry Pi 5/CM5 support; **not yet pinned**.
- Never bundle vendor boot firmware or pretend a Raspberry Pi 5 defconfig is CM5-qualified without a hardware test.

## Backend
```sh
cd services/backend
python -m pip install -r requirements.txt
python -m pytest -q
cp .env.example .env
# edit .env, then docker compose up --build -d
```

## Device
```sh
cd device/cm5-os
bash -n scripts/configure.sh
# Review README and run configure against your checked-out Buildroot tree.
# Verify Kconfig symbols with that exact release, then build and inspect output/images.
```

## Hardware acceptance evidence to capture
1. Exact CM5 SKU, carrier revision, power supply, HDMI display and SD/eMMC boot media.
2. Bootloader/kernel/Buildroot version, serial boot log, `dmesg`, HDMI mode and network status.
3. Qt/QML UI navigation by keyboard and actual Bluetooth remote.
4. Jellyfin login, poster retrieval, playback, seek, pause and audio/video sync.
5. Thermal stability under sustained playback and game load.
6. Recovery from network loss, server outage and unexpected power removal.

## Releasing
Create immutable, versioned source and binary artifacts with SHA-256 hashes, SBOM, changelog and reproducible build logs. Signed A/B OTA and rollback are future deliverables, not included here.
