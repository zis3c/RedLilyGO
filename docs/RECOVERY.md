# Crash recovery

RedLilyGO records crash-reset history for the T-Dongle-S3 board support code.
After three consecutive crash boots, firmware safe mode skips Marauder startup.
Thirty seconds of runtime clears the stored crash streak. Safe mode is a
startup guard, not a general isolation or repair mode. Firmware recovery uses
the ESP32-S3 USB bootloader.

## Safe mode behavior

The board stores the crash streak in Preferences and displays a report after a
crash-class reset. Safe mode is selected when the streak reaches three. Capture
evidence before changing files or reflashing.

When safe mode appears:

1. Stop sending payloads and disconnect optional peripherals.
2. Capture the serial log, reset reason, displayed backtrace, firmware version,
   board profile, and last change.
3. Remove or rename a recently added autorun payload using a safe SD path.
4. Rebuild the known-good environment. Flash through USB after saving the
   evidence and checking the selected board profile.

The red display report contains a saved crash backtrace. Some displayed stack
values can be incomplete or corrupted. Preserve the serial reset reason,
`EXCVADDR`, and `Backtrace:` lines for decoding. USB connect/disconnect events
alone do not prove a CPU crash, and a backtrace that does not resolve to
application code does not identify a root cause.

## USB recovery

1. Disconnect the board.
2. Hold `BOOT` while reconnecting USB.
3. Release `BOOT` when the bootloader USB device appears.
4. Run `python -m platformio device list` and identify the bootloader by its
   USB ID. Port names vary and can change after reset.
5. Flash with `.\tools\flash.ps1 -Port <port>` or an explicit PlatformIO upload command.
6. Re-enumerate after reset. Check boot logs, AP, dashboard, SD, USB, and TFT
   separately; a successful flash does not prove these runtime checks passed.

The TFT can be blank in ROM bootloader mode. This is normal because the ROM
bootloader does not initialize the display.

## Evidence record

Record firmware version, Git SHA, PlatformIO environment, image hash, reset
reason, serial log, bootloader port, application port, and health endpoint
response. Use [HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md).
