# Device-side smoke assets

These files are copied to the board or its SD card for manual hardware checks.
They are not built into the firmware and are not part of the host verification
suite in the parent `tests/` directory.

- `Devices/LILYGO-T-Dongle-S3/autorun.ds` - hardware smoke payload for the
  T-Dongle-S3. Needs the Windows Agent plus `/testcard.png` and `/disk.img` on
  the SD card. Exercises the TFT, LEDs, buttons, Marauder, USB storage and a
  reset.
- `Devices/LILYGO-T-Dongle-S3/testcard.png` - image used by the display check.
- `Features/HID/autorun.ds` - HID feature payload that types `STRING`/`STRINGLN`
  blocks and ASCII art into Notepad.

Copy a payload to the SD card as `/autorun.ds` (with autorun enabled) or run it
from the dashboard. Use only on equipment you own or are authorized to test.

Host contracts and regressions: [../README.md](../README.md).
Board evidence: [../../docs/HARDWARE_TEST_RESULTS.md](../../docs/HARDWARE_TEST_RESULTS.md).
