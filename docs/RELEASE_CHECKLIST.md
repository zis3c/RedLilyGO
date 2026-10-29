# Release Checklist

Use this file for release evidence. Contribution rules are in
`CONTRIBUTING.md`, while physical evidence is recorded in
[HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md).

Checkpoint 2026-10-02: local development candidate only, not a published
release. [LOCAL_VERIFICATION.md](./LOCAL_VERIFICATION.md) records the earlier
package; [HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md) records a
completed board smoke pass. GitHub release items remain unchecked because no tag
or release was published.

## Local preflight

- [x] Confirm version alignment for proposed `v0.1.0`; tag not created
- [x] Run `.\tools\verify.ps1` at the prior checkpoint; rerun affected checks after later changes
- [x] Install UI and browser dependencies by following `tests/README.md`
- [x] Run `npm run test:e2e` from `ui/`: 72 passed, including recorder/log/upload regressions
- [x] Run Agent protocol checks: 5 scenarios passed, unchanged Agent source
- [x] Review current Git status; preserve unrelated changes and record dirty-worktree state
- [x] Add `SECURITY.md` and a contact-request issue template without vulnerability details
- [x] Build local firmware/Agent package, manifest and verified checksums
- [x] Run release package preflight regressions
- [x] Execute the actual GitHub checksum step locally, including the empty-artifact failure case

## Hardware evidence

Board smoke pass recorded 2026-10-02. See
[HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md) for evidence and the note
that some rows are operator manual observation.

Instrumented long-duration soak evidence is tracked separately in
[HARDWARE_SOAK_EVIDENCE.md](./HARDWARE_SOAK_EVIDENCE.md). A release that claims
runtime stability needs a completed PASS row there.

- [ ] Run `.\tools\hardware-soak.ps1 -Minutes 60` and record a PASS row in
  [HARDWARE_SOAK_EVIDENCE.md](./HARDWARE_SOAK_EVIDENCE.md)

- [x] Build target `LILYGO-T-Dongle-S3` (compilation evidence only)
- [x] Flash with the explicit port shown by `python -m platformio device list`
- [x] Record esptool `Hash of data verified`
- [x] Confirm the board re-enumerates as `CAFE:403F` when using the default USB
  identity
- [x] Start the Agent and record connection/status logs
- [x] Test Remote Control in desktop and mobile-sized browser viewports
- [x] Stop and restart the Agent to verify offline and reconnect state
- [x] Test USB HID in `Serial + HID`, including reconnect after browser reload
- [x] Test screensaver toggle and activity wake-up
- [x] Test Marauder stop from dropdown and BOOT triple-click fallback
- [x] Test BLE command start, stop, and repeat without reboot
- [x] Test PCAP close, refresh, and download after capture stops
- [x] Verify firmware updates remain USB-only in source/contracts

## GitHub release evidence

- [ ] Push tag `v<version>`
- [ ] Confirm firmware and Windows Agent jobs pass
- [ ] Confirm verification job passes contracts and Playwright
- [ ] Confirm release contains firmware `.bin`, Agent `.exe`, `LICENSE`,
  `THIRD_PARTY_NOTICES.md`, both third-party license archives, `SETUP.md`,
  `release-manifest.json`, and `SHA256SUMS.txt`
- [ ] Verify the published checksum before distribution
- [x] Add user-visible changes to `CHANGELOG.md` under Unreleased
- [ ] Attach completed `docs/HARDWARE_TEST_RESULTS.md` for board-tested claims

## Independent assurance

The internal review ([SECURITY_AUDIT.md](./SECURITY_AUDIT.md)) is not an
independent audit. High-assurance releases should complete the independent
penetration test described in [PENTEST_PLAN.md](./PENTEST_PLAN.md).

- [ ] Run `.\tools\pentest-preflight.ps1` and hand the bundle plus
  [PENTEST_PLAN.md](./PENTEST_PLAN.md) to an independent tester
- [ ] Record the completed report reference in
  [SECURITY_AUDIT.md](./SECURITY_AUDIT.md) (no private report data committed)
- [ ] Confirm at least one reviewer other than the author signed off (see
  `.github/CODEOWNERS` and `CONTRIBUTING.md`)
