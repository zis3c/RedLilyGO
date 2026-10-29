# Hardware and board reference

The primary PlatformIO environment is `LILYGO-T-Dongle-S3`. The table records
the pins configured by that environment. Confirm the physical board revision
and wiring before connecting a module; a profile setting is not hardware test
evidence.

## T-Dongle-S3 pins

| Function | GPIO or value | Notes |
| --- | ---: | --- |
| TFT reset | 1 | Display control |
| TFT data/command | 2 | Display control |
| TFT MOSI | 3 | SPI output |
| TFT chip select | 4 | SPI select |
| TFT clock | 5 | SPI clock |
| TFT backlight | 38 | Display LED/backlight |
| TFT MISO | -1 | Not connected in profile |
| TFT busy | -1 | Not connected in profile |
| SD_MMC D0 | 14 | 4-bit SD_MMC |
| SD_MMC D1 | 17 | 4-bit SD_MMC |
| SD_MMC D2 | 21 | 4-bit SD_MMC |
| SD_MMC D3 | 18 | 4-bit SD_MMC |
| SD_MMC clock | 12 | SD_MMC clock |
| SD_MMC command | 16 | SD_MMC command |
| Button | 0 | Active-low GPIO button |
| Addressable LED data | 40 | One LED in current profile |
| Addressable LED clock | 39 | One LED in current profile |
| Display width/height | 160 x 80 | Logical display dimensions |
| TFT width/height | 80 x 160 | Panel orientation dimensions |

USB serial profile defaults include VID `0xCAFE`, PID `0x403F`, serial CDC mode,
and HID class. USB mode and class can be changed by settings or payloads.

This environment is configured for 8 MB flash and a project-local partition
table, `partitions_redlilygo_8mb.csv`. It uses one 6 MB factory app partition
(the two OTA slots from `default_8MB.csv` are unnecessary because OTA was
removed), plus NVS, a SPIFFS area for the Marauder no-SD fallback, and a
coredump slot. Check the installed flash size before using it. A partition
table must fit the actual flash chip; this profile is not appropriate for
every board revision.

USB can enumerate more than once during a reset. The default bootloader USB ID
is `303A:1001` and the default application ID is `CAFE:403F`; host port names
are not stable identifiers. Re-run `python -m platformio device list` before
each serial operation. A repeated reset loop or red crash report needs serial
evidence.

## Active feature flags

The current T-Dongle profile enables `HAS_SD` and `USE_SD_MMC_INTERFACE`. The
target board has TFT, SD, button, LED, WiFi, USB, and Marauder capability. It has
no microphone, touch, or IR hardware, so those drivers are not built.

Optional peripherals require a compatible board, free GPIO, voltage-safe wiring,
and a dedicated build profile. Do not enable a feature until the physical
hardware exists.

## Bootloader display behavior

When the T-Dongle-S3 is held in `BOOT` mode during USB connection, the TFT may
remain completely blank. The ESP32-S3 ROM bootloader does not initialize the
TFT, so this is normal and is not a display failure by itself.

Verify bootloader entry through USB enumeration and the COM port instead:

1. Disconnect the board.
2. Hold `BOOT` while connecting USB.
3. Release `BOOT` when the bootloader USB device appears.
4. Run `python -m platformio device list`.

The firmware logo is only expected after a normal firmware reset. Do not use
the blank TFT as the success or failure signal for download mode.

The firmware reads the ESP32-S3 internal temperature estimate every five
seconds. Its software guard reacts after five consecutive readings at or above
95 C: it stops Marauder and Wi-Fi, displays a warning briefly, then turns off
the backlight. It attempts Wi-Fi recovery when the reading is 70 C or lower.
The sensor is installed with its widest accepted range (-10..80 C); the Arduino
core default (10..50 C) extrapolates above 50 C and reports unrealistically high
values. Readings outside the sensor range are ignored, so this guard is a
last-resort backstop, not a thermostat. It is not a certified thermal
protection system and does not limit USB input current or protect against every
board-level failure.

The LED animation uses red for idle, white for error, orange for Marauder,
magenta for Ducky, cyan for VNC, yellow for HID, and green for stop/completed
feedback.
The Agent connection has a separate green indicator on the TFT; it does not
set the LED to green. Web activity wakes the TFT from the screensaver.
BOOT triple-click stops active Ducky, Marauder, and capture work and shows
stop feedback.

## Board validation

The dashboard reads device fields from firmware endpoints. A successful HTTP
response confirms that the endpoint replied; it does not validate the sensor,
storage, radio, or Agent by itself. For a board check, record the endpoint
response and separately exercise the SD card and any installed peripherals.

The current UI handles Raw Input, Agent Command, and payload queue responses
inline. The Agent Command feature requires a compatible Windows Agent. Native
USB HID controls do not require that Agent. These UI states do not prove that a
host accepted or completed the requested action; check the resulting log or
host behavior.

The Tools page currently exposes Wi-Fi packet capture and a stop command.
Capture records frames the radio receives while capture is active; it does not
guarantee that a particular target or protocol will be observed. USB-NCM
capture requires USB mode NCM and mounted storage. The firmware and DuckyScript
interfaces expose the capture controls; the main dashboard has no dedicated
USB-NCM capture page.

Evil Portal can be started after selecting a supported HTML form template in
Files. Multiple templates can coexist. The portal AP name and password come from
Settings -> Evil Portal AP (`portal-ssid` / `portal-pwd`) or the DuckyScript
launcher's `-n` / `-p` flags, and it serves on `http://4.3.2.1/`. Submissions are
written locally to
`/Captures/Portal/evilportal_submissions.jsonl` when storage is available. Use
only synthetic values in an isolated lab. The feature is not a real identity
provider or a safe destination for real credentials.

The Files page lists storage files and exposes the actions enabled for each
file type. Standard destination folders include `/Portal`, `/Scripts`,
`/Captures/WiFi`, `/Captures/Bluetooth`, `/Captures/Portal`, `/Captures/Other`,
`/Images`, `/Logs`, and `/Other`. New uploads use extension-based `Auto`
routing unless a destination is chosen. Existing root files are not migrated;
they remain at their current paths and may still appear in listings.

The logging subsystem appends entries to `/Logs/system.log` when storage is
available and closes each append. The Logs page can read a tail of the saved
file; clearing the runtime panel does not delete it. There is no automatic
rotation in the current implementation. A full or unavailable card can prevent
new disk log writes, and power loss can interrupt the current write.

After changing pins or flags:

1. Build the exact PlatformIO environment.
2. Check USB enumeration and serial boot logs.
3. Verify display, button, LED, SD mount, WiFi, and USB mode separately.
4. Test storage and capture before connecting an external module.
5. Document any GPIO conflict in the profile and release notes.

The source of truth for configured pins and compile-time capabilities is the
selected `platformio.ini` environment. It does not establish what is soldered
onto a particular board. Update this document when the profile changes and add
dated hardware evidence to [HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md)
for claims tested on a physical board.
