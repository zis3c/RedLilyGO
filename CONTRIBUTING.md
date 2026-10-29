# Contributing to RedLilyGO

## Scope and safety

The main firmware target is `LILYGO-T-Dongle-S3`. Contributions should respect
the selected board's compile-time capabilities, storage lifecycle, USB
protocol, and local-network assumptions. Exercise radio, HID, storage, and host
control features only on equipment and networks you own or are authorized to
test.

## Development setup

Install Git, Python 3 with PlatformIO Core, Node.js and npm, .NET 8 SDK, and a
USB data cable. These dependencies cover the firmware, UI, and optional Windows
Agent. Run commands from the repository root.

```powershell
.\tools\verify.ps1
```

Install the locked UI dependencies and Playwright Chromium first using
[tests/README.md](./tests/README.md). The verification script rebuilds UI assets,
runs all source contracts, JavaScript syntax checks, host firmware/tool
regressions and desktop/mobile Playwright tests, builds and checks the Agent,
then builds the selected PlatformIO environment. It does not exercise physical
peripherals; use [docs/HARDWARE_TEST_RESULTS.md](./docs/HARDWARE_TEST_RESULTS.md)
for the board record.

Report suspected vulnerabilities using [SECURITY.md](./SECURITY.md). Public bug
reports must not contain vulnerability details, credentials or private captures.
Participation in this project is covered by the
[Code of Conduct](./CODE_OF_CONDUCT.md).

## Change rules

- Edit `ui/src/`, then run `python build_ui.py` to refresh generated
  `ui/content/` files. The PlatformIO hook generates embedded headers under
  `src/html/`; do not edit either generated tree by hand.
- Preserve unrelated dirty worktree changes.
- Update the relevant machine-readable API or protocol document when a route,
  WebSocket message, CDC tag, setting, or capability changes.
- Add or update a focused contract when an observable behavior changes.
- Bound allocations, input sizes, queues, and process output.
- Preserve the current optional Basic Auth and browser cross-site checks.
  Document their limits; do not describe them as mandatory authentication or
  protection for clients that omit browser origin headers.

## Pull request checklist

- [ ] Explain behavior and affected board profiles.
- [ ] Add or update tests and documentation.
- [ ] Run the relevant contracts and build; list exact commands and results.
- [ ] Run `git diff --check`.
- [ ] Confirm no credentials, captures, or generated local paths are included.
- [ ] Record hardware-only checks separately from automated checks.

## Becoming a reviewer

The project is currently maintained by a single author, which is the largest
non-code risk to release maturity. Reviewers and second contributors are
welcome. To build review trust:

1. Land two small, focused pull requests that pass CI and the review checklist
   above (for example a contract, a payload guide, or a bounded bug fix).
2. Ask to be added to `.github/CODEOWNERS` for the area you have contributed to.
3. Review at least one change outside your own area and explain the reasoning in
   the pull request.

Reviewers are expected to check: the security controls in `SECURITY.md`, bounded
input handling, generated-asset policy, and that hardware claims separate
automated evidence from operator observation.

## Reviewing a change

- Confirm the change matches the stated behavior and affected profiles.
- Check that new routes/messages/settings update `openapi.yaml`, `asyncapi.yaml`
  and the relevant contract.
- Verify state-changing routes stay POST-only with the CSRF token, and that any
  new route is added to `requestNeedsMutationGuard`.
- Confirm generated trees (`ui/content/`, `src/html/`) are rebuilt, not edited.
- Prefer a failing test that reproduces a bug before the fix.

Use [CHANGELOG.md](./CHANGELOG.md) for release notes and
[docs/RELEASE_CHECKLIST.md](./docs/RELEASE_CHECKLIST.md) for release evidence.
Hardware soak evidence belongs in
[docs/HARDWARE_SOAK_EVIDENCE.md](./docs/HARDWARE_SOAK_EVIDENCE.md), and the
independent-test starting kit is [docs/PENTEST_PLAN.md](./docs/PENTEST_PLAN.md).
