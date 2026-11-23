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
