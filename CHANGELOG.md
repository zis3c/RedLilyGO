# Changelog

All notable changes to RedLilyGO are documented here. The format follows Keep a
Changelog and releases use Semantic Versioning.

## [Unreleased]

### Added

- Static-analysis CI: gitleaks, cppcheck, actionlint, ruff, PSScriptAnalyzer,
  eslint, Semgrep, markdownlint, lychee link checking, npm and NuGet
  vulnerability audits, and commitlint. CodeQL and dependency-review run once
  the repository is public.
- clang-tidy (`pio check`), a libFuzzer target for the HTTP input validators,
  an axe-core accessibility test, Agent coverage via coverlet, an SBOM plus
  build-provenance attestation on release, CodeQL on private repositories
  (python/javascript/csharp, plus C++), a scripted hardware soak
  (`tools/hardware-soak.ps1`), and an internal security review
  (`docs/SECURITY_AUDIT.md`).
- `.editorconfig`, `.markdownlint.json` and a pre-commit configuration.
- Contributor Code of Conduct, plus an open-source hygiene pass that removes a
  machine-specific compiler path from the tracked VS Code settings and a stray
  Blender backup file.
- Standard OpenAPI and AsyncAPI descriptions.
- Security reporting policy and local code-quality audit evidence.
- Host regressions for web authentication, WebSocket lifecycle, recorder,
  preview proxy and generated assets, plus desktop/mobile file-editor tests.
- Firmware, hardware, recovery, Ethernet, and test-result documentation.
- TFT screensaver setting, activity wake-up, and BOOT triple-click stop path
  for DuckyScript, Marauder, and USB-NCM capture.
- Compact log formatting and consistent Marauder/Raw Input/Agent mini-logs.
- Marauder dropdown stop command and BLE advertiser lifecycle protection.
- Architecture contract that guards the web module split and blocks personal
  review markers from `src/`.
- `tools/clean.ps1` and `.gitignore` coverage for the generated working tree.
- First recorded hardware evidence for a LILYGO T-Dongle-S3: verified USB CDC
  framing and Agent handshake, boot sequence, SoftAP broadcast, SD system-log
  persistence, malformed-frame parser recovery, composite USB HID, USB mass
  storage read/write, and end-to-end `AgentVNC.ds` Agent launch
  (`docs/HARDWARE_TEST_RESULTS.md`). A manual board smoke pass covered display,
  button, LED, radio, NCM capture, screensaver, stop-all, web/setup and a soak.
- `tests/static_analysis.py`, which runs GCC `-fanalyzer` over the host firmware
  groups and separates real safety findings from harness extraction artifacts.

### Changed

- Merged `examples_extended/` into the payload folder (now `payloads/`) so
  payloads live in one place, and moved the device smoke assets from `test/` to
  `tests/device/`.
- Split the HTTP service out of the 2,100-line `WebServer.cpp` into
  `WebAuth.cpp`, `WebResponse.cpp`, `WebStatic.cpp`, `WebUpload.cpp`, and
  `WebWebSocket.cpp`, sharing state through `WebInternal.h`.
- Replaced the 8 MB default partition table with `partitions_redlilygo_8mb.csv`,
  merging the unused OTA slots into a 6 MB factory app (flash use 82.9% to
  44.1%).
- Source contracts and host regressions now read the full web source set instead
  of a single filename.
- Documented the required `-Wl,-z,muldefs` workaround for vendored Marauder
  duplicate symbols.
- Consolidated documentation navigation and contribution guidance.
- Local verification now includes Playwright and all discovered source
  contracts; pull-request CI also builds/tests the Windows Agent.
- Direct firmware dependencies are pinned; unchanged generated assets are
  preserved and UI bundles publish atomically.
- `Rickroll` keeps the dashboard AP channel while channel-hopping modes may
  disconnect the dashboard client.
- USB HID status, SD usage states, dashboard typography, and TFT/LED state
  feedback are documented by their current behavior.
- Split `webRequestHandler` further: `WebServer.cpp` now owns only lifecycle,
  setup, WebSocket wiring and dispatch, while `WebRoutes.cpp` holds per-concern
  handlers (`handleInfoRoute`, `handlePayloadRoute`, `handleMediaRoute`,
  `handleMarauderRoute`, `handleStorageRoute`, `handleSystemRoute`,
  `handleStaticRoute`).
- State-changing HTTP routes are now POST-only and require a per-boot
  `X-CSRF-Token` read from `GET /csrf-token`; missing tokens return
  `403 csrf_required` and non-POST methods return `405 post_required`. The UI,
  `openapi.yaml`, preview server and tests were updated to match.
- Replaced the remaining `sprintf` calls in the Ducky USB-ID extension and the
  S3 board backtrace formatter with bounded `snprintf`.
- Added a configurable AP network address (`wifi-ap-ip`, default `4.3.2.1`).
  Captive-portal redirects and the redirect back to the dashboard now derive the
  device host from it instead of hardcoding `http://4.3.2.1`.
- Added a compiled host regression (`web_mutation_gate`) that drives the
  dispatcher's POST-only + CSRF decision matrix, read-route exemption and
  per-boot token rotation.
- Added a second libFuzzer target (`csrf_fuzz`) for the token acceptance
  invariant, and made the static analyzer run in `--strict` mode in the release
  gate and CI.
- Documented the internal security review as a non-independent assessment in
  `SECURITY.md`.
- Hardened `tools/hardware-soak.ps1` with objective PASS/FAIL criteria
  (reboots, unreachable samples, minimum free heap, peak temperature, and a
  heap-regression trend), an exit code for release gating, and JSON/CSV verdict
  evidence.
- Added `docs/HARDWARE_SOAK_EVIDENCE.md` for instrumented soak records and
  `docs/PENTEST_PLAN.md` plus `tools/pentest-preflight.ps1` to prepare an
  independent penetration test.
- Added `.github/CODEOWNERS`, a security section in the pull-request template,
  and a reviewer path in `CONTRIBUTING.md` to reduce single-author risk.
- Split the display layer: `DisplayDevice.cpp` now owns lifecycle and loop
  coordination while `DisplayScreens.cpp` renders each screen mode and
  `DisplayInternal.h` shares the panel and state machine.
- Added `tests/coverage.py`, which reports host-regression line coverage with
  GCC gcov and gates CI at a minimum floor.
- Added `docs/TLS_DEPLOYMENT.md` describing how to terminate TLS at a reverse
  proxy without changing the firmware, and the header behaviour the proxy must
  preserve.
- Replaced the Apple-branded captive-portal template with a generic
  "Mamak WiFi" page (`payloads/evilap/mamakwifi.html`) that carries no
  third-party logo or trademark and shows a visible DEMO banner, and documented
  why real-provider login clones are out of scope.
- Added `NOTICE.md` with an explicit intended-use and legal statement, and a
  `.gitleaks.toml` allowlist for synthetic test values.
- Added `tools/codeql.ps1` to run CodeQL locally over the UI, Python tooling and
  Windows Agent, so a private repository can get CodeQL findings without GitHub
  Advanced Security.

### Fixed

- `AgentVNC.ds` kept under the Windows Run command length limit and no longer
  calls `Get-Volume`, which could stall while the SD re-enumerated as USB mass
  storage.
- Pin `System.Net.Http` 4.3.4 and `System.Text.RegularExpressions` 4.3.1 to
  override vulnerable transitive versions in the Windows Agent, and repair the
  README image link.
- Documentation route and capability references aligned with current source.
- Stale file-list/preview responses, saves targeting a changed selection,
  duplicate saves and loss of edits during pending save requests.
- Upload folder names are rendered as literal option text.
- Upload requests avoid duplicates, preserve newer file selections, cancel
  stale progress timers and recover from timeout without losing selection.
- Recorder errors preserve the feed and remain visible; cursors recover after
  restart and overlapping polls are skipped.
- Old SD/runtime log responses cannot overwrite the selected view; malformed
  runtime payloads preserve displayed logs and pending refreshes are reused.
- WebSocket bridge commands are serialized with viewer ownership changes.
- Authentication lockout timer rollover and recorder callback/timing races.

- BLE spam no longer reinitializes NimBLE on every advertisement cycle.
- Duplicate log timestamps, empty component lines, and joined mini-log events
  are normalized.

### Security

- Recorder mutations reject foreign origins and token-free access requires a
  loopback Host. Preview proxy rejects foreign origins and oversized bodies.

### Removed

- The unused `include/` PlatformIO boilerplate directory; project headers live
  under `src/`.
- OTA updater references and configuration. Firmware updates use USB flashing
  only.

## Release format

Use these headings for future releases:

### Added

New features.

### Changed

Changes to existing behavior or compatibility.

### Deprecated

Features planned for removal.

### Removed

Removed features.

### Fixed

Bug fixes.

### Security

Security fixes and impact notes without exposing credentials or exploit data.
