#!/usr/bin/env python3
"""Build calendar/production.ics from calendar/events.json (stdlib only).

Event fields: id (stable, required), title, date (YYYY-MM-DD, all-day) or
start/end (YYYY-MM-DDTHH:MM, local to "timezone"), optional end_date,
location, description.
"""
import json, pathlib, datetime as dt

HERE = pathlib.Path(__file__).parent

def esc(s):
    return s.replace("\\", "\\\\").replace(";", "\\;").replace(",", "\\,").replace("\n", "\\n")

def fold(line):
    out, b = [], line.encode()
    while len(b) > 75:
        cut = 75
        while (b[cut] & 0xC0) == 0x80:
            cut -= 1
        out.append(b[:cut].decode()); b = b" " + b[cut:]
    out.append(b.decode())
    return "\r\n".join(out)

def main():
    cal = json.loads((HERE / "events.json").read_text(encoding="utf-8"))
    tz = cal.get("timezone", "UTC")
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    L = ["BEGIN:VCALENDAR", "VERSION:2.0", "PRODID:-//Wave Culture Media//Production Calendar//EN",
         "CALSCALE:GREGORIAN", "METHOD:PUBLISH", f"X-WR-CALNAME:{esc(cal['name'])}",
         f"X-WR-TIMEZONE:{tz}", "REFRESH-INTERVAL;VALUE=DURATION:PT6H", "X-PUBLISHED-TTL:PT6H"]
    for e in cal["events"]:
        L += ["BEGIN:VEVENT", f"UID:{e['id']}@wave-culture-media", f"DTSTAMP:{stamp}",
              f"SUMMARY:{esc(e['title'])}"]
        if "date" in e:
            d = dt.date.fromisoformat(e["date"])
            end = dt.date.fromisoformat(e.get("end_date", e["date"])) + dt.timedelta(days=1)
            L += [f"DTSTART;VALUE=DATE:{d:%Y%m%d}", f"DTEND;VALUE=DATE:{end:%Y%m%d}"]
        else:
            s = dt.datetime.fromisoformat(e["start"]); f = dt.datetime.fromisoformat(e["end"])
            L += [f"DTSTART;TZID={tz}:{s:%Y%m%dT%H%M%S}", f"DTEND;TZID={tz}:{f:%Y%m%dT%H%M%S}"]
        for k, p in (("location", "LOCATION"), ("description", "DESCRIPTION")):
            if e.get(k):
                L.append(f"{p}:{esc(e[k])}")
        L.append("END:VEVENT")
    L.append("END:VCALENDAR")
    (HERE / "production.ics").write_bytes(("\r\n".join(fold(x) for x in L) + "\r\n").encode())

if __name__ == "__main__":
    main()
