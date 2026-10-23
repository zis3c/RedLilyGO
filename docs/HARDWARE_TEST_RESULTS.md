# Hardware test results

Use this template for board evidence. Automated build and contract results must
not be presented as hardware validation.

Leave a result blank when a check was not run or no evidence was saved. A blank
cell is not a pass.

Current local candidate: 2026-10-02. A LILYGO T-Dongle-S3 was flashed and
exercised over USB CDC serial, USB HID, USB mass storage, and the optional
Windows Agent. A manual board smoke pass by the operator then covered display,
button, LED, radio, NCM capture, screensaver, stop-all, and the device-AP web
and setup flow. Rows marked "operator manual" are that manual observation; they
are not reproduced by the automated host checks.

## Test metadata

| Field | Value |
| --- | --- |
| Date and time | 2026-10-02, local host |
| Operator | Local automated verification plus serial and USB-host probing |
| Board model and revision | LILYGO T-Dongle-S3, ESP32-S3 QFN56 rev v0.2 |
| Firmware version | `0.1.0`, base commit `1c9f134` plus uncommitted local changes |
| Git SHA | `1c9f134` (version banner reports this base; working tree is dirty) |
| PlatformIO environment | `LILYGO-T-Dongle-S3` |
| Host OS | Windows / PowerShell |
| Application serial port and USB ID | `COM6`, `CAFE:403F` |
| Bootloader serial port and USB ID | `COM3`, `303A:1001` |
| Flash chip | Detected 16 MB (`ef 4018`); project uses the 8 MB table, which fits |
| Chip MAC | `44:1b:f6:xx:xx:xx` (last three bytes redacted) |
| Power and cable | USB-C, host port direct |

### Client devices used

Manual checks used an iPhone XS, a Samsung Galaxy A30, a Huawei MateBook D14
2021, and a Samsung Galaxy Tab A8.

## Automated evidence

| Check | Command or source | Result | Log/link |
| --- | --- | --- | --- |
| Source contracts | `tests/*_contract.mjs` (13 files) | 13 passed | Local run |
| Host firmware regressions | `python tests/firmware_regressions.py` | All groups passed | Local run |
| Static analysis | `python tests/static_analysis.py` (`g++ -fanalyzer`) | No located application defect; only harness artifacts (unused from source slicing, location-less `<unknown>` notes, mock container leak notes) | Local run |
| Firmware build | `python -m platformio run -e LILYGO-T-Dongle-S3` | Passed; RAM 33.5%, flash 44.1% of 6 MB app | Local run |
| Flash and verify | `python -m platformio run -e LILYGO-T-Dongle-S3 -t upload --upload-port COM3` | Passed; `Hash of data verified` | Local run |

## Serial hardware evidence

Firmware debug output is sent over USB CDC as framed messages (`tag` byte plus
little-endian `uint32` length plus payload). The host opened `COM6` and parsed
frames. Confirmed frames:

| Observation | Raw result | Interpretation |
| --- | --- | --- |
| App re-enumerated after flash | `USB VID:PID=CAFE:403F` on `COM6` | TinyUSB OTG app descriptor active |
| Boot sequence (tag 2 `DebugMsg`) | `BOOT Reset 1` | `ESP_RST_POWERON` (code 1), not a crash |
| WiFi bring-up | `WIFI AP + PW`, then `WIFI Web server on` | SoftAP and HTTP server started |
| System ready | `SYS Ready`, `SYS LILYGO_T_DONGLE_S3`, `SYS FW 1c9f134` | Boot completed; model and version correct |
| Persisted system log | `OLD\|00:00`..`04:51` lines replayed | SD-backed `/Logs/system.log` written and read back |
| CDC host to device | Host sent tag 8 `AgentStatus` payload `HOST-TEST\n9.9.9\n2` | Frame parsed; no malformed-stream discard |
| Agent handshake | Firmware logged `Agent\|Agent has responded with status` | Agent status callback accepted the host frame |
| Agent polling | Repeated tag 7 `RequestAgentStatus` frames | Heartbeat path runs while an Agent is known |
| Parser robustness | Sent tag 0, tag 99, an oversized length, and a truncated header, then a valid `AgentStatus` | Malformed frames discarded; the next valid frame was still accepted (`Agent has responded with status`) |
| Stability | `CAFE:403F` enumerated continuously across ~60 s and several captures | No panic, watchdog reset, or crash loop observed |

The device AP `RedlilyGO` (WPA2) was observed in a Windows `netsh wlan show
networks` scan, confirming the SoftAP broadcasts. The web UI, setup/auth flow,
radio features and NCM capture were exercised separately by the operator from a
phone on the device AP.

The firmware exposes no inbound CDC query other than `AgentStatus`/`ExecuteResult`,
so physical peripherals cannot be validated over serial alone.

## USB, storage and Agent evidence

The host enumerated the board as a composite device on `CAFE:403F`:

| Interface | Evidence | Interpretation |
| --- | --- | --- |
| CDC serial (`MI_00`) | `USB Serial Device (COM6)` | Serial transport enumerates |
| HID (`MI_02`) | `HID Keyboard Device`, `HID-compliant mouse`, consumer control, all `Status = OK` | Keyboard, mouse, and consumer HID enumerate |
| Mass storage (`MI_03`) | `USB Mass Storage Device`; SD mounted as FAT32 removable drive `D:` | MSC exposes the SD to the host |

Additional host-side checks:

| Check | Evidence | Interpretation |
| --- | --- | --- |
| HID keystrokes delivered | A DuckyScript payload typed a command that created `redlily_hid.txt` with `hid-ok` | Injected keystrokes reach the host (HID works) |
| HID with `ATTACKMODE HID STORAGE` | HID interface persisted alongside MSC and keystrokes still landed | Combined HID + storage enumerates on this firmware |
| MSC read/write | The host read `D:\RedLilyGO.Agent.exe` and wrote payload files to `D:\Scripts\` | Mass storage read and write work |
| PowerShell invocation | A payload typed `powershell -NoExit -c "..."` and it created `redlily_ps.txt` with `ps-ran` | `STRING` reaches PowerShell through Win+R |
| Agent auto-start | `AgentVNC.ds` launched `RedLilyGO.Agent.exe`; agent log recorded `Board connected on COM6` | End-to-end Agent launch and handshake |

Two payload-specific findings were recorded from this work:

- Windows Run truncates typed commands at roughly 260 characters, so a long
  `RUN`-style one-liner fails silently. `payloads/vnc/AgentVNC.ds` is kept short.
- `Get-Volume` can stall while the SD re-enumerates as a USB drive. The example
  now checks a fixed set of drive letters with `Test-Path` instead.

## Board smoke tests

| Area | Expected result | Actual result | Evidence |
| --- | --- | --- | --- |
| Boot | No panic or watchdog loop | Pass (serial) | `BOOT Reset 1` (power-on) then `SYS Ready`; stable enumeration |
| Display | TFT initializes and renders | Pass | Operator manual observation |
| Button | Short and long press work | Pass | Operator manual observation |
| LED | Status changes are visible | Pass | Operator manual observation |
| SD | Mount, list, write, read, eject | Pass (eject not exercised) | MSC mounted the SD as drive `D:`; files read and written; persisted system log replayed |
| WiFi AP | Device AP and dashboard open | Pass | AP broadcasts; operator opened the dashboard from a phone on the device AP |
| Setup/auth | Setup gate and mutation protection work | Pass | Operator completed first-run setup and authenticated from a phone |
| USB serial | CDC enumerates and Agent status works | Pass (serial) | Framed RX/TX and Agent handshake confirmed |
| USB HID | Keyboard and mouse reports work | Pass | Composite HID enumerated; injected keystrokes created host marker files |
| USB MSC | Host mount and safe removal work | Pass (removal not exercised) | SD mounted as FAT32 drive `D:`; exe read and files written |
| USB NCM | Interface and PCAP work if enabled | Pass | Operator manual test |
| Agent | Agent launches and connects over CDC | Pass | `AgentVNC.ds` launched the agent; log showed `Board connected on COM6` |
| Marauder | Scan, stop, and capture work if enabled | Pass | Operator manual test (authorized targets) |
| BLE Marauder | Start, stop, and repeat without reboot | Pass | Operator manual test |
| Screensaver | Toggle and activity wake-up work | Pass | Operator manual observation |
| Stop all | BOOT triple-click stops active features | Pass | Operator manual test |
| Long-duration stability | No reboot or watchdog during a soak | Pass | Operator manual soak |

## Failure record

No failure was observed in this session. Describe exact steps, expected result,
actual result, serial output, reset reason, screenshots, capture filename, and
whether the failure reproduced after a clean flash. Redact credentials, private
PCAP contents, and local secrets.
