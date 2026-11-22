#!/usr/bin/env bash
# Generates the short clip every title plays in the browser preview: an animated colour
# pattern with a tone, 15 s, 640x360 H.264/AAC (plays in every current browser).
set -euo pipefail
out=${1:?usage: make_demo_clip.sh <out.mp4>}
ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc2=size=640x360:rate=30:duration=15" \
  -f lavfi -i "sine=frequency=220:beep_factor=4:duration=15" \
  -vf "hue=h=t*18:s=1.4,eq=saturation=1.3" \
  -c:v libx264 -profile:v baseline -pix_fmt yuv420p -preset veryfast -crf 36 \
  -c:a aac -b:a 64k -movflags +faststart "$out"
