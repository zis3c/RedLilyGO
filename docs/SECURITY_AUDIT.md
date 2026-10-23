# Security review

**This is an internal, source-driven security review written by the project
maintainers. It is not an independent audit and not a penetration test.** A
genuine independent assessment requires a third party who did not write the
code, with authority to test a physical device and the optional Windows Agent.
Until that exists, treat the findings here as a starting point, not a
certification.

- Date: 2026-10-02
- Scope: `src/` firmware, `ui/` web UI, `tools/Agent/` Windows Agent, and the
  repository tooling in this checkout.
- Method: manual source review, the configured static analysis (cppcheck,
  Semgrep, CodeQL for python/js/csharp), the host regressions, and the libFuzzer
  targets. No third-party test, no adversarial hardware lab.
- Independent test: not yet performed. The starting kit for a third-party
  assessment is [PENTEST_PLAN.md](./PENTEST_PLAN.md); `tools/pentest-preflight.ps1`
  captures the tested surface. Any high-assurance claim requires that report.

## Deployment assumptions

The device is a local security-testing tool on a trusted, isolated network:

- The dashboard is served over plain HTTP and WebSockets (`ws://`) without TLS.
- The device runs a Wi-Fi SoftAP with a default password of `redlilygo`.
- First-run setup enables HTTP Basic Authentication; a one-time nonce and an
  origin check gate that first submission.
- Physical USB access to the board is treated as access to the attached host.

Any deployment that exposes the dashboard to an untrusted network, or shares the
AP password, invalidates most of the mitigations below.

## Attack surface and controls

| Surface | Exposure | Existing controls | Residual risk |
| --- | --- | --- | --- |
| HTTP `/data.json`, `/telemetry`, `/set`, `/system`, `/marauder`, ... | Local network | Optional Basic Auth; POST-only mutations with a per-boot `X-CSRF-Token` from `/csrf-token`; origin/`Sec-Fetch-Site` guard on mutation routes; per-IP lockout (5 failures, 30 s, 32-entry cap); input validation (`RequestValidation.h`); response bounds | Plaintext transport; origin guard is bypassable by non-browser clients that omit the headers; the token stops browser CSRF, not a hostile same-LAN client |
| WebSocket `/websockify`, `/input`, `/audio` | Local network | Handshake origin check and optional Basic Auth; single HID owner; `/input` text frames capped at 512 bytes | Missing origin metadata is allowed; unencrypted |
| USB CDC framed protocol | Attached host | One-byte tag + 4-byte length framing, 2048-byte payload cap, one-second frame deadline, malformed-stream recovery (exercised by the fuzzer and host regressions) | Inbound commands limited to Agent status/results |
| Windows Agent | Host that trusts the USB device | USB VID/PID identity match; protocol version check | VID/PID is not authentication; the Agent can run host commands by design |
| USB MSC / HID / NCM | Attached host | Mass storage exposes the SD; HID injects input; NCM captures traffic | Physical access is trusted; captures and HID payloads are sensitive |
| SD storage / uploads | Local network | `isSafeUserPath` / `validUploadFilename` reject traversal and control characters; 4 MB upload cap; upload commits atomically from a temp file | A full card can stop log writes; storage contents are unencrypted |
| Marauder radio features | Local network | Optional Marauder, explicit start/stop commands | Offensive radio behavior; authorized use only |
| Firmware update | USB only | OTA removed from the build | No remote update path to attack |

## Findings

### High (accepted design risk)

1. **Plaintext HTTP/WebSocket and cleartext Basic Auth.** Credentials and
   session content are not protected on the wire. Acceptable only on an
   isolated device network; documented in `README.md` and `SECURITY.md`.
2. **Default credentials.** The AP password defaults to `redlilygo` and the
   compiled Basic Auth fallback password is `redlilygo`. First-run setup forces
   a 12-64 character web password, but the AP password must be changed manually.

### Medium

1. **No CSRF tokens on non-browser paths.** State-changing routes now require a
   POST carrying the per-boot `X-CSRF-Token` and are checked against `Origin` or
   `Sec-Fetch-Site`. Browsers enforce the same-origin policy on the token read
   and the custom header, so cross-site pages cannot forge a mutation. A
   non-browser client on the LAN that already knows the token (or reaches the
   device directly) can still send it; the token is a browser-boundary control,
   not a network boundary.

### Low

1. **Origin metadata is optional.** Handshakes without `Origin`/`Sec-Fetch-Site`
   are accepted to support non-browser clients.
2. **Sensitive local data.** System logs, PCAP files, uploads and Agent prompts
   can contain private data and are stored unencrypted on the SD card.

## Verified controls

- Bounded buffers and frame limits across CDC and utilities; the HTTP validator
  fuzzer ran clean under AddressSanitizer.
- Authentication lockout is safe across 32-bit timer rollover, and the auth path
  has compiled behavioral checks.
- Every state-changing route is POST-only and gated by a per-boot CSRF token;
  the mutation-route set and the dispatcher gate have compiled and source
  contract checks (`tests/csrf_contract.mjs`). The upload callback enforces the
  token itself because it runs before the dispatcher.
- Path traversal, upload size, filename and directory validation are covered by
  host regressions.
- Dependency audits (npm, NuGet) and secret scanning (gitleaks) pass; CodeQL
  reports no production findings for python, javascript-typescript and csharp.
- OTA was removed, so there is no remote firmware update surface.

## Recommendations

1. Keep the device AP isolated; change the AP and web passwords before any use.
2. Prefer a WPA3/long unique AP password and avoid joining the board to shared
   networks.
3. Treat any USB-connected host as trusted; do not leave the board unattended in
   a host you do not administer.
4. Commission an **independent penetration test** and complete the instrumented
   hardware soak (`tools/hardware-soak.ps1`) before treating a deployment as
   high assurance.
