<!-- Keep pull requests focused. Redact secrets, credentials and personal data. -->

## Summary

What does this change, and why?

## Scope

Mark the areas this pull request touches.

- [ ] Firmware (`src/`, `platformio.ini`, `partitions_*.csv`)
- [ ] Web UI (`ui/`)
- [ ] Windows Agent (`tools/Agent/`)
- [ ] Payloads (`payloads/`)
- [ ] Tests, docs or tooling

## Checklist

- [ ] I ran the relevant contracts and build, and listed the results below.
- [ ] I added or updated tests and documentation for behavior changes.
- [ ] I updated the API or protocol documents for any route, CDC tag, setting,
      or capability change.
- [ ] I ran `git diff --check` and confirmed no generated local paths, secrets,
      or credentials are included.
- [ ] Hardware-only checks are recorded separately in
      `docs/HARDWARE_TEST_RESULTS.md`, not claimed from a build or CI run.

## Security review

- [ ] State-changing routes are POST-only and enforce the `X-CSRF-Token`, and
      any new route is added to `requestNeedsMutationGuard`.
- [ ] New inputs are bounded (length, count, allocation, queue) and validated.
- [ ] No secrets, tokens, or default credentials are added or logged.
- [ ] Generated trees (`ui/content/`, `src/html/`) are rebuilt, not edited.

## Verification

```text
List the exact commands you ran and their results.
```

## Related issues

Link any related issues or discussions.
