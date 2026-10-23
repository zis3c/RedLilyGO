# RedLilyGO Architecture

For the repository entry point, see [README.md](../README.md). Build and
contribution steps are in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Purpose

RedLilyGO is an embedded firmware and local-control project for ESP32-S3 boards
with four main layers:

1. hardware access
2. feature and attack logic
3. local control surfaces such as the web UI and serial Agent
4. generated assets and release verification tooling

## Runtime flow

1. `src/main.cpp` starts the device
2. board and hardware services are initialized
3. storage and Wi-Fi start before services that depend on them
4. Ducky and Agent start during setup; Marauder setup is deferred until a
   supported command requests it
5. the main loop services devices, features, and background helpers

BOOT triple-click calls the firmware stop path for DuckyScript, Marauder, and
USB-NCM capture. It also shows stop feedback on the TFT when thermal protection
is not holding the display off. Exact radio and capture cleanup depends on the
active subsystem; verify that a capture file is closed before removing storage.

The current production target is the `LILYGO-T-Dongle-S3` PlatformIO
environment. Board capabilities are selected by compile-time flags in
`platformio.ini`; unsupported peripherals must remain explicit in that profile.

## Important areas

### Core

- `src/main.cpp`
- `src/RedLilyGOCapability.*`
- `src/Utilities/Settings.*`
- `src/Diagnostics/Logging.*`

These files define boot, shared component lifecycle, persistent settings, and
runtime logging.

### Devices

`src/Devices/` contains the hardware abstraction layer.

Examples:

- `USB/` handles USB mode and class behavior
- `WiFi/` manages access point and web server bring-up
- `Storage/` mounts SD or flash-backed storage
- `Display/` owns the panel: `DisplayDevice.cpp` covers lifecycle and loop
  coordination, `DisplayScreens.cpp` renders each screen mode, and
  `DisplayInternal.h` shares the panel and state machine between them
- `LED/`, `Button/` manage user-facing hardware
- `Platform/` contains shared board lifecycle abstractions
- `Boards/LilyGO_TDongleS3/` contains T-Dongle-S3-specific platform behavior

### Features

`src/Attacks/` contains the higher-level behaviors built on top of the device layer.

- `Ducky/` executes payload scripts
- `Marauder/` bridges wireless features
- `Agent/` provides serial-side remote control helpers and host input bridging

The web handler queues Marauder commands for the feature loop. Some radio
operations change the radio channel and can interrupt a dashboard client.
Current `Rickroll` handling preserves the configured AP channel. BLE spam
reuses an advertiser during a run and stops it through the Marauder stop path.

Detailed interfaces live in [FIRMWARE_REFERENCE.md](./FIRMWARE_REFERENCE.md),
[openapi.yaml](../openapi.yaml), and [asyncapi.yaml](../asyncapi.yaml).

### Web UI

- `ui/src/` contains maintained dashboard modules and the page template.
- `ui/content/` contains generated firmware-facing HTML, CSS, and JavaScript.
- `ui/vnc/index.html` contains the project VNC page and input-only interface;
  its noVNC client libraries are third-party code.
- `build_ui.py` expands includes and combines the ordered UI source files.
- `platformio_assets.py` compresses generated content into headers under
  `src/html/` for firmware embedding.

Edit `ui/src/`, run `python build_ui.py`, then build the selected PlatformIO
environment. Do not hand-edit generated files under `ui/content/` or
`src/html/`.

### Web service

The device HTTP and WebSocket service lives under `src/Comms/Web/` and is split
by concern:

- `WebServer.cpp` owns lifecycle, route registration, and request dispatch.
- `WebAuth.cpp` owns authentication, lockout, setup nonce, and origin guards.
- `WebResponse.cpp` builds the JSON telemetry, files, dashboard, and health
  responses.
- `WebStatic.cpp` resolves embedded static assets and MIME types.
- `WebUpload.cpp` handles authenticated multipart uploads.
- `WebWebSocket.cpp` owns the VNC bridge, audio, and HID input channels.
- `WebInternal.h` declares the state and helpers shared across those units.

`tests/lib/web_source.mjs` and `tests/firmware_regressions.py` read the whole
directory, so source checks do not depend on a single filename.

### PC-side tooling

`tools/Agent/` contains the optional .NET Windows application for USB serial
transport, VNC screen sharing, remote input, and explicitly requested host
commands.

The Agent is optional. Firmware and native USB HID continue without it, while
VNC screen sharing, Agent status, and Agent-backed commands need a compatible
Agent connection. The local macro recorder under `tools/macro-recorder/` is a
separate developer utility and is not part of firmware runtime.

### Verification and release

- `tests/` contains behavior contracts that run without hardware.
- `tools/verify.ps1` is the local release gate and includes strict static
  analysis.
- `.github/workflows/main.yml` repeats source, UI, Agent, and firmware checks.
- `docs/HARDWARE_TEST_RESULTS.md` records physical-board smoke evidence.
- `docs/HARDWARE_SOAK_EVIDENCE.md` records instrumented long-duration soak runs.
- `docs/PENTEST_PLAN.md` and `tools/pentest-preflight.ps1` prepare an
  independent penetration test; the internal review is not a substitute.

Passing CI proves source and build consistency. It does not prove board boot,
peripheral behavior, radio behavior, or long-duration stability.

Use [RELEASE_CHECKLIST.md](./RELEASE_CHECKLIST.md) for release evidence and
[HARDWARE_TEST_RESULTS.md](./HARDWARE_TEST_RESULTS.md) for board evidence.

The dashboard renders values supplied by firmware endpoints. A missing sensor,
unmounted storage, empty scan, or disconnected Agent can produce an unavailable
or empty state; the UI does not establish that the physical feature works.

The TFT screensaver is enabled by default and is controlled from Settings.
Activity from the web, Ducky, or Marauder path calls `wakeForActivity()` so a
new state is shown immediately.

### Third-party code

- `lib/ESP32Marauder/`
- `lib/usb-ncm/`
- `ui/vnc/vendor/`

These paths include bundled dependencies. Check each component's local license
and copyright notices before changing or redistributing its code.

## Recommended mental model

- If it talks to hardware, look in `src/Devices/`
- If it changes behavior, look in `src/Attacks/`
- If it changes screens or pages, look in `ui/`
- If it changes build or board selection, look in `platformio.ini`
- If it changes generated web assets, check `platformio_assets.py`
