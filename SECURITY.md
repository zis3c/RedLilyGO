# Security policy

## Maintained scope

Security fixes target the current development branch, the primary
`LILYGO-T-Dongle-S3` firmware profile, bundled web/Remote UI, Windows Agent,
and repository tools. Older releases and other board profiles may require an
upgrade or a reproduction on the maintained profile. This project does not
promise a vulnerability response deadline or certify production security.

## Reporting a vulnerability

Use GitHub's **Report a vulnerability** button on the repository's
[Security page](https://github.com/zis3c/RedLilyGO/security) when available.
This creates a private report; do not use public bug issues for exploit details.

If private reporting is unavailable, open a public issue titled
`Private security contact requested`. Include only the affected component and
a request for a private reporting channel. Wait for the maintainer to arrange
that channel before sharing reproduction steps, logs or sensitive details.
Private reporting availability has not been confirmed for this repository;
the policy does not assume that the GitHub button is enabled.

A private report should include:

- Firmware version, Git commit and whether local changes are present.
- Board/profile, Agent version and host OS where relevant.
- Affected component, expected behavior, observed impact and minimal steps.
- Synthetic test data and redacted evidence sufficient to reproduce the issue.

Remove passwords, tokens, personal data and third-party network captures.
Reproduce only on equipment and networks where you have permission.

The maintainer can request more information, coordinate a fix and agree on
disclosure with the reporter. A published fix should identify affected
versions, impact and upgrade instructions without exposing private report data.

## Deployment assumptions

- Default HTTP/WebSocket traffic has no TLS. Use a controlled device network;
  do not expose the dashboard directly to the Internet.
- First-run setup enables Basic Authentication. It can later be disabled;
  origin checks do not replace network isolation or authentication.
- State-changing routes require an HTTP POST and the per-boot `X-CSRF-Token`
  from `GET /csrf-token`. The token blocks browser cross-site requests and
  cached-Basic-auth GET prefetch. It is not a network boundary: any client that
  can already reach the device and read the token can still call the routes.
- Change the default AP password before sharing access, and change the default
  AP address (`wifi-ap-ip`, default `4.3.2.1`) if the deployment requires a
  different subnet.
- USB CDC access to the Agent is a host-control trust boundary. USB VID/PID
  values do not authenticate a connected device cryptographically.
- Firmware updates use USB flashing. Logs, PCAP files and payloads may contain
  sensitive data and require appropriate storage and deletion.

## Independent review

The project maintains an internal, source-driven security review at
[docs/SECURITY_AUDIT.md](./docs/SECURITY_AUDIT.md). It is not an independent
audit or a penetration test and must not be treated as one. A genuine
independent assessment by a party that did not write the code, with authority to
test a physical device and the optional Windows Agent, remains outstanding and
is the main remaining security-assurance gap.

See [README.md](./README.md#security-posture) for the current security posture
and [docs/RECOVERY.md](./docs/RECOVERY.md) for recovery instructions.
