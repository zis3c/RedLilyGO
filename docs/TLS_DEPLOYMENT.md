# TLS and reverse-proxy deployment

The default firmware serves plain HTTP and WebSockets (`ws://`) on the device
AP with no TLS. That is a deliberate choice for a local, isolated testing tool:
it keeps the flash and RAM footprint small and avoids certificate management on
an offline device. `SECURITY.md` and `docs/SECURITY_AUDIT.md` document the
resulting risk.

This guide describes the supported way to add transport encryption when a
deployment needs it, without changing the firmware.

## Why not on the device

- The ESP32-S3 has no trusted clock without NTP, so certificate expiry and
  handshake validity are unreliable on an isolated AP.
- The device image is USB-flashed only; rotating an embedded certificate means
  reflashing.
- TLS termination would not add real value for the intended single-user,
  isolated-AP use.

Keep the risk low with the documented controls instead: short-lived sessions,
the default-isolated AP, a unique WPA2/WPA3 password, and no Internet exposure.

## Recommended pattern: TLS-terminating reverse proxy

Put a reverse proxy on a host that has a clock and a certificate, and let it
forward to the device. This keeps the firmware unchanged.

```text
browser --(https/wss)--> reverse proxy --(http/ws)--> device AP (4.3.2.1)
```

### Caddy (automatic certificates)

```caddyfile
redlilygo.example.lan {
    reverse_proxy 4.3.2.1:80
}
```

Caddy terminates TLS and proxies WebSocket upgrades automatically. Replace
`redlilygo.example.lan` with a name that resolves to the proxy host.

### nginx

```nginx
server {
    listen 443 ssl;
    server_name redlilygo.example.lan;

    ssl_certificate     /etc/ssl/certs/redlilygo.crt;
    ssl_certificate_key /etc/ssl/private/redlilygo.key;

    location / {
        proxy_pass http://4.3.2.1:80;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $remote_addr;

        # WebSocket upgrade for /websockify, /input and /audio.
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

## Required proxy behaviour

The firmware's browser-boundary controls depend on a few headers. A proxy that
breaks them weakens the deployment:

- **Preserve `Origin`.** The CSRF and cross-site guards compare `Origin` against
  the `Host` the browser used. Rewriting or dropping it can reject legitimate
  mutations or, worse, mis-evaluate a cross-site attempt. Set
  `proxy_set_header Host $host;` and do not strip `Origin`.
- **Forward upgrade headers** so `/websockify`, `/input` and `/audio` work. The
  VNC bridge needs a working WebSocket upgrade.
- **Do not cache.** The device already sends `no-store` for dynamic routes.
- **Pass through `Set-Cookie`/`Authorization`** if web authentication is enabled.

## Behaviour when proxied

- `GET /csrf-token` returns the token through the proxy. The token is per-boot
  and only readable from the same origin, so the browser boundary still holds.
- Origin checks compare the browser's `Origin` to the `Host` it requested. When
  the proxy forwards the original `Host`, the check passes; when it rewrites the
  `Host`, the browser's `Origin` no longer matches and mutations are rejected.
  Keep `Host` intact.
- The CSP header is served by the firmware and applies at the proxy response. If
  the proxy rewrites the page origin, update the CSP `connect-src` accordingly,
  or let the proxy add its own CSP.

## Verify the proxy

1. Confirm `https://<name>/health` and `https://<name>/data.json` load.
2. Confirm WebSocket channels connect (Remote Control and audio).
3. Confirm a mutation (for example Reboot) succeeds, which proves `Origin`/`Host`
   are compatible with the CSRF guard.
4. Confirm a request with a foreign `Origin` is rejected.

Record the proxy configuration and certificate handling with the deployment. TLS
at the proxy protects the wire; it does not replace an independent penetration
test or the device-side checks.
