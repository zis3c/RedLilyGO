# Changelog

All notable changes to RedLilyGO are documented here. The format follows Keep a
Changelog and releases use Semantic Versioning.

## [v0.1.1] - 2026-10-29

### Fixed

- Prevented RGB status LED and TFT display wake-up on remote or tool activity (Marauder, USB HID, VNC, DuckyScript) when the device is manually turned off via the BOOT button.
- Enforced complete RGB status LED suppression during manual screen-off state regardless of web stealth mode setting.

## [v0.1.0] - 2026-10-28

### Added

- ESP32-S3 firmware combining USB HID keyboard/mouse, mass storage (MSC), serial CDC, and optional USB-NCM networking.
- ESP32 Marauder Wi-Fi and Bluetooth assessment workflows with PCAP packet capture to SD card.
- Local SoftAP web dashboard with real-time telemetry, responsive controls, and live system log stream.
- DuckyScript execution engine with hardware-level extensions for display, storage, and wireless triggers.
- Companion .NET 8 Windows Agent over serial CDC for authorized lab testing and remote screen streaming.
- ST7735 TFT display driver with dynamic status dashboard, log viewer, screensaver, and payload selector.
- APA102 RGB status LED with smooth breathing cadence and multi-feature activity indication.
- Hardware-backed crash recovery, safe mode reboot counter, and dual flash storage partitions.
- Static analysis test suites: cppcheck, gitleaks, Semgrep, CodeQL, and comprehensive host regression tests.

### Changed

- Calibrated APA102 RGB LED hardware color response for accurate amber-orange representation.
- Multi-activity LED breathing now smoothly alternates across concurrent tasks (Marauder + Ducky, Marauder + HID) at cycle minimum brightness.
- Active feature status on TFT dashboard prioritizes newly activated interactive tasks.
- Remote VNC web input automatically reconnects when Wi-Fi state transitions occur during radio scans.
- Hardened HTTP mutation endpoints with per-boot CSRF tokens and POST-only validation.

### Fixed

- Eliminated LED freeze when running concurrent Marauder and DuckyScript sessions.
- Resolved premature idle-red LED reverts when stopping secondary USB HID inputs during active Marauder operations.
- Prevented radio mode conflicts and port 80 socket bind collisions during Evil Portal initialization.
- Guarded background BLE scan allocations and resolved heap fragmentation on long-running sweeps.
- Hardened WebAuth session timing and fixed token rollover race conditions.

### Security

- Enforced loopback host requirements for local recorder and preview endpoints.
- Replaced third-party branded portal assets with generic demonstration templates.
- Pinned direct dependencies and audited third-party packages against known vulnerability databases.
