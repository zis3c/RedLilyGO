# Firmware reference

This guide describes the firmware behavior represented in this checkout. The
selected PlatformIO environment and compile-time flags determine which parts
are present. For network shapes, see [openapi.yaml](../openapi.yaml) and
[asyncapi.yaml](../asyncapi.yaml). For setup and board details, see
[SETUP.md](./SETUP.md) and [HARDWARE.md](./HARDWARE.md).

## Build profile and capabilities

The default `LILYGO-T-Dongle-S3` environment in `platformio.ini` selects the
T-Dongle-S3 pin map, the 8 MB project partition table
(`partitions_redlilygo_8mb.csv`), SD_MMC storage, TFT, button, LED, Wi-Fi, USB,
and Marauder code. These are build settings, not proof that a particular
assembled board has working hardware. Other profiles can enable different
devices.

Capabilities in the dashboard reflect compile-time support and some runtime
state. Check the selected environment, wiring, initialization result, and live
device response before relying on a capability.

## Local HTTP and WebSocket service

In AP mode, the default device address is `http://4.3.2.1/`. The default AP
name is `RedLilyGO` and its configured password is `redlilygo`; change that
password before sharing the device. The firmware serves HTTP and WebSocket
without TLS in this profile, so keep access on a trusted, isolated network.

Basic Auth is optional and defaults to disabled. First-run setup asks for a
username and a 12 to 64 character password, then enables Basic Auth. An owner
can later change the setting. When Basic Auth is disabled, the web handlers do
not require credentials. Browser mutation routes check supplied `Origin` or
`Sec-Fetch-Site` headers, but clients that omit both are allowed. These checks
do not encrypt traffic or provide a complete authorization boundary.

Every state-changing route requires an HTTP POST that carries the per-boot
`X-CSRF-Token` header, read once from `GET /csrf-token`. A request with a
non-POST method is answered with `405 post_required`; a missing or stale token
is answered with `403 csrf_required`. The token blocks browser cross-site
requests and cached-Basic-auth GET prefetch, because a cross-site page cannot
read `/csrf-token` or set the header without a preflight the device never
approves. It is not encryption. Clients should not cache, prefetch, or
automatically retry any request that changes state, runs a payload, captures
traffic, or reboots/formats the device.

### HTTP routes

| Route | Purpose and important limits |
| --- | --- |
| `GET /csrf-token` | Returns the per-boot CSRF token required in the `X-CSRF-Token` header of every mutation. |
| `GET /setup/status` | Returns whether first-run setup is required. Before setup, includes the current nonce. |
| `POST /setup` | Accepts credentials and the current nonce while Basic Auth is disabled. Username is 3 to 32 letters, numbers, underscores, or hyphens; password is 12 to 64 characters. Requires a same-origin `Origin` header. It returns 409 while Basic Auth is enabled. |
| `GET /health` | Reports firmware-defined Wi-Fi, storage, heap, USB, Agent, reset, and safe-mode fields. `healthy` does not validate every peripheral or feature. |
| `GET /data.json` | Dashboard snapshot, settings metadata, file details, runtime logs, and capability state. Sensitive setting values are masked. |
| `GET /telemetry` | Smaller live telemetry response that avoids the full SD file snapshot. |
| `GET /runtime-logs` | Recent in-memory log entries. This is a bounded runtime view, not the persistent log file. |
| `GET /systemlog?tail=N` | Text tail of the persistent log. Default is 200 lines; accepted range is 1 to 2000. Requires mounted storage. |
| `POST /clearlogs` | Clears the in-memory log panel buffer. It does not erase the saved SD log. |
| `GET /files` | Bounded recursive file snapshot for compatibility. |
| `GET /files?directory=/path` | Lists one folder's immediate files and child folders. The Files page loads folders on demand to keep each response small. A folder with more than 24 direct files or child folders is marked truncated. |
| `GET /download?filename=...` | Streams a file from device storage. |
| `POST /uploadFile` | Multipart upload. Supports standard-folder or automatic placement and controlled replacement. See OpenAPI parameters. |
| `POST /delete?filename=...` | Deletes a storage file. |
| `POST /moveFile?from=...&to=...` | Moves a file while retaining its basename. |
| `POST /runfile?filename=...` | Queues a `.ds` file from storage for execution. A success response means queued, not completed. |
| `POST /rawinput?rawCommand=...` | Queues a Ducky command or a recognized Marauder command. A queue response does not confirm execution. |
| `POST /runagentcmd?rawCommand=...` | Queues a command through a connected, protocol-compatible Windows Agent. |
| `POST /showimage?filename=...` | Displays a PNG from storage on builds with a display. |
| `POST /marauder?marauderCmd=...` | Queues a Marauder command when Wi-Fi and Marauder are ready. |
| `POST /capture?source=wifi&action={start,stop}` | Starts or stops the Wi-Fi raw capture workflow when supported. |
| `POST /capture?source=usb&action={start,stop}` | Starts or stops USB NCM capture when NCM and storage are ready. |
| `POST /mic?enabled={true,false}` | Starts or stops microphone capture on a build with a configured microphone. |
| `POST /system?cmd=reset\|reboot\|format` | Resets settings, reboots, or formats storage. These operations are destructive. |
| `POST /settings?...` | Changes the dashboard toggles or Wi-Fi mode. GET requests are rejected. Wi-Fi changes reboot to apply. |
| `POST /set?name=...&value=...` | Changes a registered setting. Omitting `value` removes a stored key where allowed. |
| `GET /wifi/scan` | Returns the current Marauder access-point snapshot when available. |
| `GET /evilportal/files` | Lists portal templates available to the firmware. |
| `GET /evilportal` | Reads portal state. |
| `POST /evilportal?action=start\|stop\|clear` | Changes portal state; start accepts a template `file` and optional `ssid`. Use synthetic data only. |
| `GET /deauth` | Returns event counters collected while the defensive `sniffdeauth` parser is active. It is not a general network audit. |
| `POST /channelhop?delay=1..60` | Sets Marauder's channel-hop delay in seconds. |

Every `POST` mutation above also requires the `X-CSRF-Token` header.

The dashboard also serves static pages at `/`, `/remote`, and `/remote/input`.
Route availability can vary by build and runtime state. See response errors and
the selected firmware source for exact behavior.

### WebSocket channels

The firmware exposes `/websockify`, `/input`, and `/audio`.
Handshake checks inspect Origin or Fetch Metadata headers when supplied and
apply optional Basic Auth. Missing origin metadata is not rejected by that
check. The default device service uses plain `ws://`, without TLS.

- `/websockify` relays VNC data between the browser and a connected Agent. A
  compatible Agent and USB CDC connection are required.
- `/input` accepts validated HID action JSON when the USB HID interface is
  ready. The firmware grants one active input owner at a time.
- `/audio` sends microphone PCM from a build with microphone support. The
  T-Dongle-S3 target has no microphone.
USB VID/PID values identify the configured USB descriptors. They do not prove
device identity, pair the host with the board, or authenticate the Agent.

## Dashboard, logs, and storage

`/data.json` is the broad UI snapshot and may perform SD-backed work. The
dashboard uses `/telemetry` for frequently refreshed values. Neither endpoint
is a substitute for direct feature testing.

The firmware keeps a bounded in-memory runtime log and queues persistent log
entries for SD writes when storage is available. The pending write queue is
bounded; under sustained storage errors, older pending entries can be dropped.
Each successful append is flushed and closed. The preferred path is
`/Logs/system.log`; older layouts may use `/Captures/system.log`. The UI clear
button clears only the runtime panel. Check `/systemlog` or the SD file before
claiming a persistent log was removed. If storage is missing, full, or fails,
log persistence may be incomplete while the firmware continues.

New user files are placed in logical folders such as `/Portal`, `/Scripts`,
`/Captures/WiFi`, `/Captures/Bluetooth`, `/Captures/Portal`, `/Captures/Other`,
`/Images`, `/Logs`, and `/Other`. Existing root-level files remain supported
for compatibility. Use the synchronized Storage APIs in new firmware code.

USB NCM PCAP files use a numbered name under `/Captures/Other/`, for example
`/Captures/Other/usbncm_0.pcap`. Marauder captures use their own configured
capture paths. Close capture before removing the card or changing USB mode,
then inspect the output with a PCAP reader. Capture files can contain private
traffic.

## DuckyScript and Marauder

Ducky payloads use `.ds` files. When `autorun` is enabled, `/autorun.ds` is
eligible to run during startup. USB host enumeration and application launch
times vary, so payloads must choose their own waits and verify results where
possible.

Firmware adds commands such as `DISPLAY_PNG`, `DISPLAY_TEXT`, `LED`,
`SET_SETTING_*`, `USB_NCM_PCAP_ON`, `USB_NCM_PCAP_OFF`, `AGENT_RUN`, and
`WAIT_FOR_AGENT_RUN_RESULT`. Optional hardware and build flags affect which
commands work. See the command registrations in `src/Attacks/Ducky/Extensions.h`
and check `IS_*` or connection predicates before relying on optional devices.

Marauder is a third-party library integrated by this firmware. Its command
syntax and behavior can change with the pinned library revision. Use its
bundled help and version-matched source for details. Only run radio tests on
owned equipment or an explicitly authorized isolated lab. Some commands
transmit frames, change radio channels, interrupt the dashboard connection,
or write sensitive data to storage. Stop commands and BOOT triple-click are
best-effort controls, not a substitute for a controlled test setup.

## Settings

Values below are code-level defaults for this checkout. A saved value in
Preferences takes precedence, and a build can omit a setting when its owning
component is disabled.

| Name | Type | Default | Meaning in this build |
| --- | --- | --- | --- |
| `wifi-ap-mode` | bool | `true` | Start in AP mode; `false` selects station mode. |
| `wifi-ap` | string | `RedLilyGO` | Configured network name used by the Wi-Fi component. |
| `wifi-pwd` | string | `redlilygo` | AP password; in station mode this setting is also used as the saved Wi-Fi password. |
| `web-auth` | bool | `false` | Enable HTTP Basic Auth. First-run setup changes this to `true`. |
| `web-user` | string | `admin` | Default HTTP username before setup changes it. |
| `web-pass` | string | `redlilygo` | Compiled fallback. First-run setup requires a password of at least 12 characters before enabling auth. |
| `wifi-bootstate` | bool | `true` | Requested Wi-Fi state at startup. |
| `portal-ssid` | string | `RedLilyGO-Portal` | Evil Portal AP name; overridden by the launcher's `-n`. |
| `portal-pwd` | string | empty | Evil Portal AP password (8-63 chars). Empty means an open AP; overridden by the launcher's `-p`. |
| `usbDeviceType` | uint16 | `Serial` | USB device mode enum value. |
| `usbClassType` | uint16 | `HID` | USB class enum value. |
| `usbDeviceVID` / `usbDevicePID` | uint16 | `0xCAFE` / `0x403F` | Default USB descriptor identifiers, not authentication. |
| `usbSerialRaw` | bool | `false` | Raw CDC serial mode. |
| `agentPolling` | bool | `true` | Enable Agent status polling. |
| `agentPollSec` | int16 | `15` | Agent status poll interval in seconds. |
| `pcapNcmOnStart` | bool | `false` | Start NCM PCAP when NCM initializes. |
| `loggerdelay` | bool | `true` | Delay selected serial log output until a debugger is connected. |
| `tftTextSize` | int16 | `10` | Display text sizing value used by this firmware. |
| `screensaver` | bool | `true` | Enable the TFT screensaver after about 60 seconds without activity. |
| `stealth` | bool | `false` | Suppress the status LED animation. It does not hide radio or USB activity. |
| `autorun` | bool | `true` | Allow `/autorun.ds` to run at startup. |
| `powersave` | bool | `false` | Select 80 MHz when enabled; the normal setting is 240 MHz. |

The web settings endpoint validates values by setting type. Unsigned 16-bit
values use hexadecimal input in the UI. Wi-Fi mode changes restart the board;
USB mode changes require USB re-enumeration or a reset depending on the caller.

## USB CDC and Agent transport

CDC frames contain a one-byte command tag, a four-byte little-endian payload
length, and the payload bytes. Current tags are defined in
`src/Devices/USB/USBCDC.h`; both peers must use compatible protocol versions.
Readers must handle partial serial reads and enforce the implementation's
frame-size limits.

The Windows Agent provides desktop capture, VNC relay, HID reporting support,
and host-side command handling. Its command path can run Windows commands, so
install and use it only on a host you control. USB VID/PID and CDC presence are
not cryptographic authentication or pairing.

Current firmware command tags are:

| Tag | Name | Firmware-side purpose |
| ---: | --- | --- |
| 1 | `Execute` | Host command or action request |
| 2 | `DebugMsg` | Debug text |
| 3 | `WSCONNECT` | Notify Agent that a VNC browser connected |
| 4 | `WSDATA` | Forward VNC data to the Agent |
| 5 | `WSDISCONNECT` | Notify Agent that the VNC browser disconnected |
| 6 | `WSDATARECV` | Forward VNC data from the Agent |
| 7 | `RequestAgentStatus` | Ask the Agent for status |
| 8 | `AgentStatus` | Agent status reply |
| 9 | `ExecuteResult` | Host command result |
| 10 | `MicPcmData` | Microphone PCM frame |

The exact byte values and frame checks are maintained in
`src/Devices/USB/USBCDC.h` and the Agent protocol implementation. Keep both
peers in sync when changing this list.

## Thermal guard and recovery

The firmware samples the ESP32-S3 internal temperature estimate every five
seconds. At or above 95 C for five consecutive samples, it stops Marauder and
Wi-Fi, shows a warning, and powers off the display. Wi-Fi recovery is attempted
when the reading falls to 70 C or lower. This software guard is not a calibrated
board-temperature measurement or certified thermal safety mechanism.

The driver is installed directly with the sensor's widest accepted range
(-10..80 C). The Arduino core default only covers 10..50 C and extrapolates
above that, which reported values far above the real die temperature. Readings
outside -20..125 C or above the sensor ceiling are discarded, so the 95 C guard
rarely trips on this part; treat it as a last-resort backstop, not a thermostat.

For the T-Dongle-S3 build, three crash-class reset boots enable safe mode. A
stable boot clears the stored crash streak after 30 seconds. This mechanism
helps collect recovery evidence; it does not identify every possible crash or
guarantee recovery. See [RECOVERY.md](./RECOVERY.md).

## UI source and generated assets

Edit browser source under `ui/src/`. Run `python build_ui.py` to generate
`ui/content/`; the PlatformIO hook `platformio_assets.py` then combines and
compresses assets into headers under `src/html/` for firmware embedding. Do
not hand-edit generated bundle files when changing the UI.

## Source of truth

Firmware behavior is defined by the selected `platformio.ini` environment,
compile flags, source under `src/`, and the pinned components under `lib/`.
`openapi.yaml` and `asyncapi.yaml` describe the network interfaces. Update this
guide when those behaviors change, and keep build, flash, and physical-device
evidence clearly separate.
