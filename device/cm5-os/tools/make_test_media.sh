#!/usr/bin/env bash
# Generates small synthetic clips for tools/fake_jellyfin.py --media <dir>:
#   <dir>/direct.mp4      H.264/AAC MP4 (the direct-play path)
#   <dir>/hls/main.m3u8   H.264/AAC in MPEG-TS segments (the shape of a Jellyfin HLS transcode)
# Needs ffmpeg. Test pattern and tone only; no third-party content.
set -euo pipefail
OUT="${1:?usage: make_test_media.sh <dir>}"
mkdir -p "$OUT/hls"
SRC=(-f lavfi -i "testsrc2=size=640x360:rate=25" -f lavfi -i "sine=frequency=440:sample_rate=48000" -t 12)
ENC=(-c:v libx264 -preset veryfast -profile:v high -pix_fmt yuv420p -g 50 -c:a aac -b:a 96k)
ffmpeg -loglevel error -y "${SRC[@]}" "${ENC[@]}" -movflags +faststart "$OUT/direct.mp4"
ffmpeg -loglevel error -y "${SRC[@]}" "${ENC[@]}" -f hls -hls_time 3 -hls_playlist_type vod \
  -hls_segment_filename "$OUT/hls/main%d.ts" "$OUT/hls/main.m3u8"
echo "test media in $OUT"
