# Code quality notes

This file records what the maintainer has reviewed and verified. It does **not**
assign the project a score, a percentage, or a rating. Numbers like that are not
meaningful here: the maintainer is not a professional firmware reviewer, there
is no independent assessment, and most quality claims cannot be validated
without physical hardware and third-party review. Treat this as an engineering
log, not a verdict.

For measured evidence, prefer the automated checks in
[LOCAL_VERIFICATION.md](./LOCAL_VERIFICATION.md), the board record in
[HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md), and the soak record in
[HARDWARE_SOAK_EVIDENCE.md](./HARDWARE_SOAK_EVIDENCE.md).

## Known limitations

- No independent penetration test has been performed. An internal source review
  ([SECURITY_AUDIT.md](./SECURITY_AUDIT.md)) exists but is not a substitute.
- Long-duration behavior is not fully characterized. A soak run should be
  recorded before claiming runtime stability.
- The project has a single author; there is no second reviewer yet.
- The host regression suite exercises selected logic against in-memory doubles.
  It does not model FreeRTOS scheduling, real SD timing, or radio behavior.
- Coverage reported by `tests/coverage.py` measures only the extracted host
  logic, not the whole firmware or any runtime path.

## What has been reviewed

### Structure

- The HTTP service is split across `WebAuth`, `WebResponse`, `WebStatic`,
  `WebUpload`, `WebWebSocket`, `WebRoutes` and a small `WebServer` dispatcher,
  sharing state through `WebInternal.h`.
- The display layer is split between `DisplayDevice.cpp` (lifecycle and loop
  coordination), `DisplayScreens.cpp` (per-mode rendering) and
  `DisplayInternal.h`.
- `tests/architecture_contract.mjs` guards both splits so they cannot silently
  collapse back into a single file.

### Security-relevant behavior

- All state-changing HTTP routes require POST and a per-boot `X-CSRF-Token`
  (`tests/csrf_contract.mjs`, the `web_mutation_gate` host group, and the
  `csrf_fuzz` target).
- Authentication has a bounded, rollover-safe lockout with compiled behavioral
  checks.
- Storage paths, upload names, upload size and command lengths are validated and
  covered by host regressions.
- OTA update was removed from the build, so there is no remote firmware path.

### Verification

- Source contracts check route/API alignment, the web and display splits, and
  security invariants without hardware.
- Host regressions compile and run selected production logic against doubles.
- `tests/static_analysis.py --strict` runs GCC `-fanalyzer` over the host
  groups and currently reports no located project findings.
- `tests/coverage.py` reports host-logic line coverage with gcov.
- CI runs cppcheck, clang-tidy (advisory), CodeQL, Semgrep, gitleaks, markdown
  and link checks, dependency audits, and two libFuzzer targets.

## How to read a change to this file

If a new check lands, add it under the relevant heading with the command that
runs it. If a limitation is removed, say what evidence removed it. Do not add a
score or a percentage.
