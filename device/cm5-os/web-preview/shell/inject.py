#!/usr/bin/env python3
"""Turns the page Qt generates for a WebAssembly app into the preview's index.html.

Usage: inject.py <generated.html> <out/index.html>

Qt's page already loads the app into a full-window canvas. This only sets the title and
adds a small banner (demo data notice + keyboard help), so it keeps working when the Qt
version and its page template change.
"""
import sys
from pathlib import Path

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
here = Path(__file__).resolve().parent
html = src.read_text()
head = (here / "head.html").read_text()
body = (here / "banner.html").read_text()

start, end = html.find("<title>"), html.find("</title>")
if start >= 0 and end > start:
    html = html[:start] + "<title>Retro Arcade preview</title>" + html[end + len("</title>"):]
for marker, extra in (("</head>", head), ("</body>", body)):
    i = html.rfind(marker)
    if i < 0:
        sys.exit(f"inject.py: {marker} not found in {src}")
    html = html[:i] + extra + html[i:]
dst.write_text(html)
print(f"wrote {dst}")
