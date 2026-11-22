#!/usr/bin/env python3
"""Minimal fake Jellyfin server for host development, UI screenshots and Qt unit tests.

Implements only the endpoints cm5-home calls, with invented sample titles and generated
artwork. Standard library only. Not a Jellyfin implementation; do not expose it.

    python3 tools/fake_jellyfin.py [--port 0]      # prints "LISTENING <port>"

Login: user "arcade", password "insert-coin". GET /__requests returns the request log
(method, path, query, Authorization header, body) for tests.

POST /Items/{id}/PlaybackInfo evaluates the client's DeviceProfile against invented media
metadata (MEDIA below), the way Jellyfin's StreamBuilder does in outline: direct play when a
DirectPlayProfile and every CodecProfile condition match, otherwise an HLS TranscodingUrl.
With --media DIR (see tools/make_test_media.sh) it also serves real streams: DIR/direct.mp4 for
/Videos/{id}/stream and DIR/hls/ for the transcode playlist. Item m7 claims to direct play but
serves corrupt bytes, to exercise the player's transcode fallback.
"""
import argparse
import json
import re
import struct
import sys
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlencode, urlsplit

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
MEDIA_DIR = None
CORRUPT_DIRECT = {"m7"}

# Invented technical metadata per item: container, video codec/profile/size/bit depth, audio codec/channels.
def _v(container, vcodec, profile, w, h, depth, acodec, ch):
    return {"Container": container, "Video": {"Codec": vcodec, "Profile": profile, "Width": w, "Height": h, "BitDepth": depth},
            "Audio": {"Codec": acodec, "Channels": ch}}


MEDIA = {
    "m1": _v("mp4", "h264", "High", 1920, 1080, 8, "aac", 2),        # direct play
    "m2": _v("mkv", "hevc", "Main 10", 3840, 2160, 10, "eac3", 6),   # 4K: transcode (resolution)
    "m3": _v("webm", "av1", "Main", 1920, 1080, 8, "opus", 2),       # AV1: transcode (codec)
    "m4": _v("avi", "mpeg4", "Simple", 720, 480, 8, "mp3", 2),       # AVI: transcode (container)
    "m5": _v("mkv", "h264", "High 10", 1920, 1080, 10, "aac", 2),    # 10-bit H.264: transcode (bit depth)
    "m6": _v("mkv", "vc1", "Advanced", 1920, 1080, 8, "ac3", 6),     # VC-1: transcode (codec)
    "m7": _v("mp4", "h264", "Main", 1280, 720, 8, "aac", 2),         # direct play that fails on the device
    "a1": {"Container": "mp3", "Audio": {"Codec": "mp3", "Channels": 2}},
    "a2": {"Container": "flac", "Audio": {"Codec": "flac", "Channels": 2}},
    "a3": {"Container": "asf", "Audio": {"Codec": "wmav2", "Channels": 2}},  # WMA: transcode
}
for _series in ("s1", "s2"):
    for _n in range(1, 5):
        MEDIA[f"{_series}e{_n}"] = _v("mkv", "hevc", "Main", 1920, 1080, 8, "aac", 2)


def _csv(value):
    return [x.strip().lower() for x in (value or "").split(",") if x.strip()]


def _check(cond, actual):
    want = cond.get("Value", "")
    if actual is None:
        return not cond.get("IsRequired", False)
    op = cond.get("Condition")
    if op == "LessThanEqual":
        return float(actual) <= float(want)
    if op == "GreaterThanEqual":
        return float(actual) >= float(want)
    if op == "EqualsAny":
        return str(actual).lower() in [x.lower() for x in want.split("|")]
    if op == "Equals":
        return str(actual).lower() == want.lower()
    if op == "NotEquals":
        return str(actual).lower() != want.lower()
    return True


def evaluate(item_id, profile, allow_direct):
    """Returns (direct_play_ok, transcode_reasons, transcoding_profile)."""
    media = MEDIA[item_id]
    kind = "Video" if "Video" in media else "Audio"
    video, audio = media.get("Video", {}), media["Audio"]
    reasons = []
    matches = [p for p in profile.get("DirectPlayProfiles", []) if p.get("Type") == kind]
    by_container = [p for p in matches if media["Container"] in _csv(p.get("Container"))]
    if not by_container:
        reasons.append("ContainerNotSupported")
    else:
        if kind == "Video" and not any(video["Codec"] in _csv(p.get("VideoCodec")) or not p.get("VideoCodec") for p in by_container):
            reasons.append("VideoCodecNotSupported")
        if not any(audio["Codec"] in _csv(p.get("AudioCodec")) or not p.get("AudioCodec") for p in by_container):
            reasons.append("AudioCodecNotSupported")
    props = {"Width": video.get("Width"), "Height": video.get("Height"), "VideoBitDepth": video.get("BitDepth"),
             "VideoProfile": video.get("Profile"), "AudioChannels": audio["Channels"]}
    reason_for = {"Width": "VideoResolutionNotSupported", "Height": "VideoResolutionNotSupported",
                  "VideoBitDepth": "VideoBitDepthNotSupported", "VideoProfile": "VideoProfileNotSupported",
                  "AudioChannels": "AudioChannelsNotSupported"}
    for cp in profile.get("CodecProfiles", []):
        applies = (cp.get("Type") == "Video" and kind == "Video" and video["Codec"] in _csv(cp.get("Codec"))) or \
                  (cp.get("Type") in ("VideoAudio", "Audio") and (not cp.get("Codec") or audio["Codec"] in _csv(cp.get("Codec"))))
        if not applies:
            continue
        for cond in cp.get("Conditions", []):
            if not _check(cond, props.get(cond.get("Property"))):
                r = reason_for.get(cond.get("Property"), "UnknownVideoStreamInfo")
                if r not in reasons:
                    reasons.append(r)
    if not allow_direct and not reasons:
        reasons.append("DirectPlayError")
    tp = next((p for p in profile.get("TranscodingProfiles", []) if p.get("Type") == kind), None)
    return not reasons, reasons, tp


def playback_info(item_id, body, authorization):
    req = json.loads(body or "{}")
    device = re.search(r'DeviceId="([^"]*)"', authorization or "")
    profile = req.get("DeviceProfile") or {}
    allow_direct = req.get("EnableDirectPlay", True)
    session = f"ps-{item_id}-{len(LOG)}"
    media = MEDIA[item_id]
    direct, reasons, tp = evaluate(item_id, profile, allow_direct)
    source = {"Id": item_id, "Container": media["Container"], "Protocol": "File", "SupportsDirectPlay": direct,
              "SupportsDirectStream": direct, "SupportsTranscoding": True, "TranscodeReasons": reasons}
    if not direct and tp:
        q = {"DeviceId": device.group(1) if device else "", "MediaSourceId": item_id, "PlaySessionId": session,
             "AudioCodec": _csv(tp.get("AudioCodec"))[0] if tp.get("AudioCodec") else "",
             "TranscodeReasons": ",".join(reasons)}
        if tp.get("Type") == "Video":
            q["VideoCodec"] = _csv(tp.get("VideoCodec"))[0]
            source["TranscodingUrl"] = f"/videos/{item_id}/master.m3u8?" + urlencode(q)
            source["TranscodingSubProtocol"] = "hls"
        else:
            source["TranscodingUrl"] = f"/audio/{item_id}/stream.{tp.get('Container', 'mp3')}?" + urlencode(q)
            source["TranscodingSubProtocol"] = "http"
        source["TranscodingContainer"] = tp.get("Container")
    return {"MediaSources": [source], "PlaySessionId": session}


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

    def _authed(self, query=None):
        # Media URLs carry the token as a query parameter, because the player cannot set headers.
        if query and (query.get("api_key") == [TOKEN] or query.get("ApiKey") == [TOKEN]):
            return True
        return f'Token="{TOKEN}"' in (self.headers.get("Authorization") or "")

    def _media(self, path, query):
        """Real bytes for host playback tests, when --media is given."""
        if m := re.fullmatch(r"/(?:Videos|Audio)/([\w-]+)/stream", path):
            if m.group(1) in CORRUPT_DIRECT:
                self._send(200, b"\x00not a video\x00" * 4096, "video/mp4")
                return True
            f = MEDIA_DIR / "direct.mp4" if MEDIA_DIR else None
            self._send(200, f.read_bytes(), "video/mp4") if f and f.exists() else self._send(404, {"error": "no media"})
            return True
        if m := re.fullmatch(r"/videos/([\w-]+)/(master\.m3u8|main\.m3u8|[\w-]+\.ts)", path):
            f = MEDIA_DIR / "hls" / ("main.m3u8" if m.group(2) == "master.m3u8" else m.group(2)) if MEDIA_DIR else None
            if not f or not f.exists():
                self._send(404, {"error": "no media"})
                return True
            if f.suffix == ".ts":
                self._send(200, f.read_bytes(), "video/mp2t")
                return True
            # Jellyfin carries the original query (and token) on every playlist and segment URL.
            qs = urlsplit(self.path).query
            lines = [ln if ln.startswith("#") or not ln.strip() else f"{ln}?{qs}" for ln in f.read_text().splitlines()]
            self._send(200, "\n".join(lines).encode() + b"\n", "application/vnd.apple.mpegurl")
            return True
        return None

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
        if not self._authed(query):
            return self._send(401, {"error": "unauthorized"})
        if method == "GET" and self._media(path, query):
            return None
        if method == "DELETE" and path == "/Videos/ActiveEncodings":
            return self._send(204, b"")
        if method == "POST" and (m := re.fullmatch(r"/Items/([\w-]+)/PlaybackInfo", path)):
            if m.group(1) not in MEDIA:
                return self._send(404, {"error": "not found"})
            return self._send(200, playback_info(m.group(1), body, self.headers.get("Authorization")))
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

    def do_DELETE(self):
        self._handle("DELETE")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8096)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--media", type=Path, help="directory from tools/make_test_media.sh")
    args = ap.parse_args()
    global MEDIA_DIR
    MEDIA_DIR = args.media
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"LISTENING {httpd.server_address[1]}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    sys.exit(0)


if __name__ == "__main__":
    main()
