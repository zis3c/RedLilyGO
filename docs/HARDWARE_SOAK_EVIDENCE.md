# Hardware soak evidence

This file records instrumented long-duration soak runs. It complements the
manual board pass in [HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md); a
build or CI pass is not soak evidence. A blank row is not a pass.

The soak is an important missing piece of evidence, because it is the only check
that measures heap stability, thermal behavior and reboot resistance over time
on real hardware.

## How to produce evidence

1. Flash the exact build under test and join the device AP.
2. If web authentication is enabled, have a credential ready.
3. Run:

   ```powershell
   .\tools\hardware-soak.ps1 -WebUri http://4.3.2.1 -Minutes 60
   # With Basic Auth:
   .\tools\hardware-soak.ps1 -WebUri http://4.3.2.1 -Credential (Get-Credential) -Minutes 120
   ```

4. The script writes `dist/hardware-evidence/soak-<stamp>.json` and `.csv`, prints
   a PASS/FAIL verdict, and exits non-zero on FAIL.
5. Record the verdict, the evidence filenames and the summary values below.

## Pass criteria

A run passes only when all of the following hold (`tools/hardware-soak.ps1`
enforces these and prints the failing reasons):

| Criterion | Default threshold | Meaning |
| --- | --- | --- |
| Reboots | 0 | The firmware-tracked minimum free heap never rose between samples |
| Unreachable samples | ≤ max(2, 10% of samples) | The device stayed responsive |
| Lowest free heap | ≥ 4096 bytes | No observed exhaustion |
| Peak temperature | < 95 C | Below the firmware thermal shutdown limit |
| Heap regression | ≥ -8192 bytes over the window | No leak-like downward trend |

Tune the thresholds only with justification; changing them to force a pass
invalidates the evidence.

## Soak runs

| Date (UTC) | Firmware / commit | Board | Minutes | Samples | Reboots | Min free heap | Peak temp C | Heap trend | Verdict | Evidence file |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| | | | | | | | | | | |

## Follow-up

Record any FAIL, the failing reason, and whether a fix and re-run reproduced a
PASS. Do not claim long-duration stability without a completed PASS row above.
