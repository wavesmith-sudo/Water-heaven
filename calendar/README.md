# Production calendar

Subscribable iCalendar feed for the Wave Culture Media production schedule.

- Feed: `calendar/production.ics`
- Edit events in `calendar/events.json`, then run `python3 calendar/build_ics.py`
  (or just push: the "Build production calendar" workflow rebuilds the feed).

Subscribe URL (after merge to `main`):

    webcal://raw.githubusercontent.com/wavesmith-sudo/water-heaven/main/calendar/production.ics

Google Calendar: Other calendars → + → From URL, paste the `https://` form of the link.
Subscribed calendars refresh on the client's schedule (Google: every few hours up to ~24h).

## Schedule page

`calendar/index.html` shows the schedule and reads `events.json` next to it, so it can be
served as-is by any static host (the Cloudflare web preview serves this folder at `/schedule/`).
`_headers` makes Cloudflare serve the feed as `text/calendar`.
`_redirects` adds the short addresses `/shoots` (page) and `/shoots.ics` (feed); like
`_headers`, it must be merged into the site root's `_redirects`.

## Media tab

The page's Media tab lists `media.json` links. Put HTML pages for it in `calendar/media/`
and link them as `media/<file>.html`; a link with `"url": null` shows as "Coming soon".

## Clipper tab

The Clipper tab is a launcher for the Clipper creator studio (separate repo, `wavesmith-sudo/clipper`).
It opens the address in `clipper.json`, which defaults to `http://localhost:5173`, Clipper's local
Docker address. Clipper is private, so do not commit its private hostname here; anyone can set
their own address on the tab, and it is kept in that browser's local storage only. The tab links
out instead of embedding Clipper, because Cloudflare Access login pages refuse to load in a frame.
