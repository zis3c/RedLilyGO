# Notice on intended use

RedLilyGO is a hardware security-testing tool. It includes USB HID injection,
Wi-Fi and Bluetooth assessment (ESP32 Marauder), captive-portal demonstrations,
and an optional Windows Agent that can run host commands.

**These capabilities can cause harm if misused. Use them only on systems,
devices, and networks you own or have explicit written permission to test.**

## Not supported

The project is not intended, and must not be used, for:

- Accessing, collecting, or transmitting credentials you do not own.
- Unauthorized access, persistence, stealth, surveillance, or disruption.
- Deploying a copy of any real provider's login page (phishing).
- Attacking networks or devices without authorization.

The bundled captive-portal templates are deliberately generic: they imitate no
real company and carry no third-party logo or trademark. Replacing them with a
real provider's login page is phishing and is outside the project's scope.

## Legal

Laws vary by jurisdiction. Unauthorized access to computer systems and networks
is a criminal offence in many countries. You are responsible for complying with
the law and for obtaining authorization before any test.

## Reporting misuse

Report security issues per [SECURITY.md](./SECURITY.md). Report trademark or
branding concerns to the maintainers rather than opening a public issue with
reproduction details.
