# Example: Power Off

This DuckyScript example types a Windows shutdown command into the connected
host. It opens the Run dialog (`Win+R`), types `shutdown /s /t 0`, and presses
Enter to power the machine off immediately. Host behavior depends on USB HID
mode, keyboard layout, focus, and host policy. Use it only on equipment you own
or are explicitly authorized to test.

## Setup

Copy `PowerOff.ds` to `/Scripts/` on the SD card, then run it from the payload
list. Use USB Serial + HID for the host keyboard portion and review the payload
before enabling autorun.

## Notes

- For a Linux or macOS host, replace the typed command with `poweroff` or
  `shutdown -h now` respectively.
- A short `DELAY` after opening the Run dialog keeps the keystrokes from
  outpacing the host, which can drop characters on slow machines.
