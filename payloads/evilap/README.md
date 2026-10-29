# Example: Local captive portal test

Captive-portal demonstration for an isolated, authorized lab. The board starts
its own AP and serves a local page so you can rehearse the portal flow with
synthetic data. Do not use it to collect real credentials.

The templates here are deliberately generic. They imitate no real company and
carry no third-party logo or trademark. Do not replace them with a copy of a
real provider's login page; that would be phishing and is not what this project
supports.

## Files

- `PortalDemo.ds` - DuckyScript launcher. Turns off the dashboard web server,
  sets the portal AP name, and starts the Marauder Evil Portal.
- `redlilygo-demo.html` - synthetic page. Its button sends only fixed dummy
  values (`demo@example.test` / `DEMO_ONLY`) to `/get`.
- `mamakwifi.html` - generic "Mamak WiFi" sign-in layout for isolated lab
  demonstrations. No real branding.
- `campus-terms.html` - generic campus Wi-Fi terms-and-sign-in layout (wide
  translucent dark card on a light page, small centered inputs, cyan button).
  No real branding or contact details.
- `campus-status.html` - generic hotspot status page (welcome, IP/bytes/uptime
  table, log-off and continue buttons) with synthetic session values. No real
  branding, domains, or contact details.

## What the launcher does

```text
WEB_OFF                                              # stop the dashboard web server (frees port 80)
ESP32M ssid -a -n RedLilyGO-Demo                     # portal AP name
ESP32M evilportal -c start -w /Portal/redlilygo-demo.html   # page to serve
```

The Marauder portal AP name comes from the `ssid -a -n` value (it takes
priority over a selected AP or `/ap.config.txt`). To serve a different page,
copy it to the SD card under `/Portal/` and change the `-w` path, for example
`-w /Portal/mamakwifi.html`.

The portal AP is open by default. Add `-p <password>` (8-63 characters) to the
`evilportal` line for WPA2, or leave `-p` out for an open network:

```text
ESP32M evilportal -c start -w /Portal/redlilygo-demo.html -p rahsia123
```

## How to test

1. Copy the page to the SD card under `/Portal/`, for example
   `/Portal/redlilygo-demo.html`. The Files page uploads to `/Portal` when you
   choose that folder.
2. Create `/Scripts/PortalDemo.ds` (or edit it on the SD card) and confirm the
   `-w` path and `-n` name you want.
3. Run `PortalDemo.ds` from the dashboard payload list. To run it at boot
   instead, copy it to the SD root as `/autorun.ds` and enable autorun.
4. The dashboard web server stops (expected). The board now broadcasts the
   portal AP, `RedLilyGO-Demo` by default.
5. On a second test device, connect to `RedLilyGO-Demo`. The captive portal
   should open automatically; if not, browse to `http://4.3.2.1/`.
6. On the demo page, press **Send test data**. The submission is stored locally
   at `/Captures/Portal/evilportal_submissions.jsonl`.
7. Stop the portal with a **BOOT triple-click** (or **Stop active scan/attack**
   if the dashboard is reachable). The dashboard AP returns.

## Notes

- The portal and the dashboard both use port 80, so the dashboard is
  unavailable while the portal runs. That is why the launcher calls `WEB_OFF`.
- If the page is not served, check that it is under `/Portal/`, contains a
  `<form>`, and is smaller than the firmware HTML limit.
- Use only synthetic test values. Real credentials must not be entered.
