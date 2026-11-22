#!/usr/bin/env python3
"""Minimal fake Jellyfin server for host development, UI screenshots and Qt unit tests.

Implements only the endpoints cm5-home calls, with invented sample titles and generated
artwork. Standard library only. Not a Jellyfin implementation; do not expose it.

    python3 tools/fake_jellyfin.py [--port 0]      # prints "LISTENING <port>"

Login: user "arcade", password "insert-coin". GET /__requests returns the request log
(method, path, query, Authorization header, body) for tests.
"""
import argparse
import json
import re
import struct
import sys
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

TOKEN = "fake-token-0123456789abcdef"
USER_ID = "u-arcade"
PALETTE = [(233, 91, 255), (41, 232, 242), (255, 210, 63), (255, 63, 164), (63, 220, 124), (120, 90, 255)]

MOVIES = [
    ("m1", "Neon Harbor", "A courier races across a flooded city before the tide turns.", 0),
    ("m2", "The Last Arcade", "Two rivals keep a shuttered game hall alive for one more summer.", 42.0),
    ("m3", "Midnight Circuit", "A street racer discovers her car remembers every road.", 0),
    ("m4", "Pixel Lighthouse", "A keeper signals ships with an old 8-bit display.", 0),
    ("m5", "Static Bloom", "A radio host hears tomorrow's weather tonight.", 0),
    ("m6", "Chrome Valley", "A ranch family raises robot horses on the high desert.", 0),
    ("m7", "Afterglow Express", "The last overnight train hides a museum car.", 0),
]
SERIES = [("s1", "Signal Lost", "A crew of night-shift engineers keeps a city's broadcasts alive."),
          ("s2", "High Score Diaries", "Contestants in a regional arcade league, season by season.")]
EPISODES = {
    "s1": [(f"s1e{i}", t, 1, i) for i, t in enumerate(["Carrier Wave", "Dead Air", "Test Pattern", "Sign Off"], 1)],
    "s2": [(f"s2e{i}", t, 1, i) for i, t in enumerate(["Continue?", "Extra Life", "Boss Rush"], 1)],
}
TRACKS = [("a1", "Sunset Drive", "Grid Runners"), ("a2", "Cartridge Heart", "Grid Runners"), ("a3", "Night Market", "Low Battery")]

LOG = []


def png(w, h, rgb):
    """Solid poster with a lighter band, as a PNG, without third-party libraries."""
    rows = []
    for y in range(h):
        c = tuple(min(255, v + 60) for v in rgb) if h * 0.62 < y < h * 0.7 else tuple(int(v * (0.55 + 0.45 * y / h)) for v in rgb)
        rows.append(b"\x00" + bytes(c) * w)
    raw = zlib.compress(b"".join(rows), 6)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", raw) + chunk(b"IEND", b"")


def item(id_, name, type_, overview="", pct=0.0, **extra):
    d = {"Id": id_, "Name": name, "Type": type_, "Overview": overview,
         "UserData": {"PlayedPercentage": pct, "PlaybackPositionTicks": int(pct * 60 * 10_000_000), "Played": False}}
    d.update(extra)
    return d


class Handler(BaseHTTPRequestHandler):
    server_version = "FakeJellyfin/0.1"

    def log_message(self, *args):
        pass

    def _send(self, code, body=b"", ctype="application/json"):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _authed(self):
        return f'Token="{TOKEN}"' in (self.headers.get("Authorization") or "")

    def _handle(self, method):
        parts = urlsplit(self.path)
        query = parse_qs(parts.query)
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length).decode() if length else ""
        path = parts.path
        if path == "/__requests":
            return self._send(200, LOG)
        LOG.append({"method": method, "path": path, "query": parts.query, "authorization": self.headers.get("Authorization"), "body": body})

        if method == "POST" and path == "/Users/AuthenticateByName":
            creds = json.loads(body or "{}")
            if creds.get("Username") == "arcade" and creds.get("Pw") == "insert-coin":
                return self._send(200, {"AccessToken": TOKEN, "User": {"Id": USER_ID, "Name": "arcade"}})
            return self._send(401, {"error": "bad credentials"})
        if not self._authed():
            return self._send(401, {"error": "unauthorized"})
        if path.startswith("/Sessions/"):
            return self._send(204, b"")
        if m := re.fullmatch(r"/Items/([\w-]+)/Images/Primary", path):
            n = sum(map(ord, m.group(1)))
            return self._send(200, png(80, 120, PALETTE[n % len(PALETTE)]), "image/png")
        if path == f"/Users/{USER_ID}/Items/Resume":
            return self._send(200, {"Items": [item(i, n, "Movie", o, p) for i, n, o, p in MOVIES if p]})
        if path == f"/Users/{USER_ID}/Items":
            kind = query.get("IncludeItemTypes", [""])[0]
            if kind == "Movie":
                items = [item(i, n, "Movie", o, p) for i, n, o, p in MOVIES]
            elif kind == "Series":
                items = [item(i, n, "Series", o) for i, n, o in SERIES]
            elif kind == "Audio":
                items = [item(i, n, "Audio", "", SeriesName=None, AlbumArtist=a) for i, n, a in TRACKS]
            else:
                items = []
            return self._send(200, {"Items": items, "TotalRecordCount": len(items)})
        if m := re.fullmatch(r"/Shows/([\w-]+)/Episodes", path):
            eps = [item(i, n, "Episode", f"Episode {e} of the season.", 0, ParentIndexNumber=s, IndexNumber=e)
                   for i, n, s, e in EPISODES.get(m.group(1), [])]
            return self._send(200, {"Items": eps})
        return self._send(404, {"error": "not found"})

    def do_GET(self):
        self._handle("GET")

    def do_POST(self):
        self._handle("POST")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8096)
    ap.add_argument("--host", default="127.0.0.1")
    args = ap.parse_args()
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"LISTENING {httpd.server_address[1]}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    sys.exit(0)


if __name__ == "__main__":
    main()
