# Local release evidence

Recorded on 2026-10-01 for `LILYGO-T-Dongle-S3`, version `0.1.0`. A 2026-10-02
follow-up supersedes the build and hardware parts of this record: the source
suite now has 13 contracts, flash use is 44.1% on the project partition table,
and a board smoke pass was completed. The candidate package and its hashes below
remain the 2026-10-01 record.

Base Git commit: `b44f10227d731f1c573146df57fb53d4614b1441`.
The working tree contains uncommitted changes. The commit alone does not
identify the built source; candidate artifacts are identified by their hashes.
The proposed tag `v0.1.0` passed version alignment checks; no tag was created.

## Automated results

Latest code-quality checkpoint: 72 desktop/mobile browser cases, all source
contracts, asset-builder regressions and the firmware build passed. Logs:
`.pio/quality-upload-browser.log`, `.pio/quality-upload-contracts.log`,
`.pio/quality-upload-firmware.log`. At the 2026-10-01 checkpoint the firmware RAM
was 109,680 bytes (33.5%) and flash was 2,772,329 bytes (82.9%); the 2026-10-02
rebuild uses 109,688 bytes of RAM and 44.1% of the 6 MB app partition. Recorder
checks passed at the preceding
checkpoint; recorder code is unchanged here. The candidate below remains the
earlier package with its original checksum; it lacks the latest log/upload fixes.
The table below records that earlier release-candidate checkpoint.

| Check | Result | Evidence |
| --- | --- | --- |
| Source contracts | 11 passed; rerun after release-document updates | `tests/*_contract.mjs` |
| Browser | 42 desktop/mobile cases passed | `.pio/quality-next-browser.log` |
| Host firmware | 13 compiled groups passed, plus source assertions | `.pio/quality-next-host.log` |
| Recorder | Passed retention, cursors, sessions, timing and API regressions | `tests/macro_server_regressions.py`, previous checkpoint |
| Agent protocol | 5 scenarios passed, unchanged Agent source | `.pio/quality-final-verify.log`, previous checkpoint |
| Agent distribution | Self-contained win-x64 publish passed | `.pio/quality-release-publish.log` |
| Firmware | Build passed; RAM 33.5%, flash 82.9% | `.pio/quality-next-firmware.log` |
| Release version | Proposed `v0.1.0` matches firmware and Agent metadata | `tools/release-check.ps1 -Tag v0.1.0` |
| Package preflight | Missing input, existing output and outside-dist output rejected | `tests/release_package_regressions.ps1` |
| Package integrity | All file sizes, manifest hashes and checksums verified; license ZIP integrity passed | Candidate below |
| GitHub checksum step | Executed locally in Git Bash: empty artifacts rejected; policy and manifest included; checksum verification passed | Actual `Generate artifact checksums` workflow block, temporary fixture |

Local logs and `dist/` output are ignored build evidence, not files bundled with
the source repository. Run [tests/README.md](../tests/README.md) checks again
when source or dependencies change.

## Candidate package

Directory: `dist/RedLilyGO-v0.1.0-local-20261001-135551`.

Contains `firmware.bin`, `bootloader.bin`, `partitions.bin`,
`RedLilyGO.Agent.exe`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `SECURITY.md`,
`SETUP.md`, `THIRD_PARTY_LICENSES.zip`, `release-manifest.json` and
`SHA256SUMS.txt`. The license archive contains 865 entries; archive integrity
does not certify license compliance.

Firmware SHA-256:

```text
c4b8abf50eb1c04f744e0ba49f54bf4ef21dff33d3057615a94bc4420c0fd03e
```

The manifest records `gitDirty: true` and `hardwareTested: false`.
The candidate was assembled using the existing packager with explicit inputs:

```powershell
.\tools\package-release.ps1 `
  -BuildDirectory .pio\quality-build `
  -AgentPath dist\local-firmware-evidence-20261001-135551\agent\RedLilyGO.Agent.exe `
  -OutputDirectory dist\RedLilyGO-v0.1.0-local-20261001-135551
```

The packager refuses an existing output directory. Choose a fresh directory
when repeating this command. Publishing to GitHub remains a separate step.

## Hardware and GitHub status

- The 2026-10-01 candidate was not board-tested. A separate 2026-10-02 pass did
  flash and exercise the board; see
  [HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md).
- The latest inspected successful PlatformIO workflow was
  [run 33856574440](https://github.com/zis3c/RedLilyGO/actions/runs/33856574440),
  for commit `8b1b8067514d7bfa1cb3274eb9fca51d0fc9725b`. It does not validate
  this dirty working tree. Remote CI and published artifact checks are pending.
- GitHub private vulnerability reporting could not be confirmed: its API
  returned HTTP 404. [SECURITY.md](../SECURITY.md) provides a public contact
  request fallback that excludes vulnerability details.
