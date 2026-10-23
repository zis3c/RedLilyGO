# Verification checks

Run source contracts from the repository root. They inspect source or generated
contracts and do not execute firmware on a board.

```powershell
node .\tests\ui_contract.mjs
```

Run security source contracts:

```powershell
node .\tests\security_contract.mjs
```

Run recovery source contracts:

```powershell
node .\tests\recovery_contract.mjs
```

Check the USB-flashing policy and optionally run the board build/flash helper:

```powershell
node .\tests\ota_contract.mjs
.\tools\hardware-smoke.ps1
```

With the computer joined to the board AP, check `/setup/status` and, when
web authentication is enabled, `/health` with credentials:

```powershell
.\tools\hardware-smoke.ps1 -WebUri http://4.3.2.1 -Credential (Get-Credential)
```

Verify release tag/version alignment:

```powershell
.\tools\release-check.ps1 -Tag "v$((Get-Content VERSION).Trim())"
```

The UI contract checks required markup IDs, Remote Control actions, and
generated-asset source wiring. These checks are intentionally dependency-free.

Firmware and companion checks:

```powershell
dotnet build .\tools\Agent\Agent.csproj --configuration Release
python -m platformio run -e LILYGO-T-Dongle-S3
```

Run Agent protocol checks:

```powershell
dotnet run --project .\tools\Agent.Tests\Agent.Tests.csproj --configuration Release
```

Run host firmware regressions with a C++17 compiler (`g++`, or set `CXX` to
its executable path):

```powershell
python .\tests\firmware_regressions.py
```

Run tool and build checks without starting a keyboard listener:

```powershell
python .\tests\macro_server_regressions.py
python .\tests\build_assets_regressions.py
python .\tests\preview_server_regressions.py
node .\tests\dependency_contract.mjs
.\tests\release_package_regressions.ps1
```

Run the architecture guard, the API contract and the static analyzer:

```powershell
node .\tests\architecture_contract.mjs
node .\tests\api_contract.mjs
python .\tests\static_analysis.py
```

The architecture contract fails if the web service collapses back into fewer
modules, if `WebServer.cpp` regresses past 1,100 lines, or if personal review
markers reappear under `src/`. The API contract checks that every firmware HTTP
route appears in `openapi.yaml` and every WebSocket channel appears in
`asyncapi.yaml`. The static analyzer runs GCC `-fanalyzer` over the host-compiled
groups and separates located findings from harness artifacts.

The host regression script compiles selected production logging, upload, and
file-commit code against in-memory device doubles. It covers the scenarios
listed by that script, but does not exercise a physical SD card, board timing,
or FreeRTOS scheduling. The WebSocket group executes the production bridge
callback with device doubles, including first/last viewer notifications,
rejected/duplicate/stale clients, exact binary forwarding and the ownership
lock required while queuing bridge commands. Recorder regressions check the
5,000-event retention limit and sequence/cursor behavior across a clear.

The PowerShell release regression uses dummy files under `dist/`; it verifies
that missing inputs, an existing package and an output outside `dist/` are
rejected before packaging. It does not publish a release or test hardware.

Flash only when the board is connected:

```powershell
.\tools\flash.ps1
```

Run the complete local verification flow:

Install the locked UI dependencies and Playwright Chromium first using the
commands below. The verification flow runs browser checks as well as all
source contracts, host regressions, Agent checks and the firmware build.

```powershell
.\tools\verify.ps1
```

Run desktop and mobile browser tests after installing the locked npm
dependencies and the Playwright Chromium browser:

Recorder cases exercise HTTP/semantic failures, preserved feeds, cursor recovery,
late responses after clear and polling overlap. Log cases exercise competing SD
downloads, runtime responses arriving after SD selection, clear invalidation and
failure preservation. These run the maintained scripts with controlled API
responses; they do not install global keyboard hooks or require a physical SD.
Upload cases use an XHR double to check duplicate requests, newer selections,
progress timers and timeout/retry cleanup. Runtime polls also check overlap and
malformed payload preservation; these do not measure real network throughput.

```powershell
cd ui
npm ci
npx playwright install chromium
npm run test:e2e
```
