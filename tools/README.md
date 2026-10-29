# Developer tools

Run these PowerShell scripts from the repository root. A successful build or
flash command is not evidence that the physical board's peripherals work.

## Verify source and builds

```powershell
.\tools\verify.ps1
```

This regenerates UI assets, runs source contracts and JavaScript syntax checks,
the host firmware/tool regressions, the desktop and mobile Playwright suite,
builds and runs Agent protocol checks, then builds the selected PlatformIO
environment. It does not test a physical board.

## Build and run the Windows Agent

Publish a self-contained x64 package, then launch it:

```powershell
.\tools\publish-agent.ps1
.\tools\start-agent.ps1
```

The executable is written under `dist/agent-win-x64/`. The Agent searches for
the configured USB VID and PID; the firmware's default identity is
`CAFE:403F`.

To use a custom identity, pass the same hexadecimal VID and PID used by the
firmware:

```powershell
.\tools\start-agent.ps1 -Vid 1234 -Pid 5678
```

Agent diagnostics are written to
`%LOCALAPPDATA%\RedLilyGO\agent.log`. Treat this local diagnostic data as
sensitive and redact it before sharing.

## Build, inspect, and flash a board

```powershell
python -m platformio device list
.\tools\hardware-smoke.ps1
```

`hardware-smoke.ps1` builds firmware and checks for the default application USB
ID when a board is connected. It does not test display, storage, radio, HID, or
other peripherals. Flashing requires the explicit `-Flash` switch:

```powershell
.\tools\hardware-smoke.ps1 -Flash
```

For multiple serial devices, choose the current application port with
`-Port`. Port names can change after reset. The script records a JSON evidence
file under `dist/hardware-evidence/`. A `Hash of data verified` message shows
that the image was written; boot and feature checks still need separate evidence.

## Check the live web service

Join the device AP, then run:

```powershell
.\tools\hardware-smoke.ps1 -WebUri 'http://4.3.2.1' -Credential (Get-Credential)
```

The script checks `/setup/status` and, when credentials are supplied, `/health`.
It does not browse the dashboard or verify individual peripherals. On a device
where web authentication is disabled, omit `-Credential`.

Keep Wi-Fi and web credentials, API keys, screenshots, and private captures out
of pasted logs and issue reports.

## Run CodeQL locally

CI runs CodeQL through the GitHub Action, but on a private repository that
result cannot be uploaded to the Security tab without GitHub Advanced Security.
Run it locally against the same components instead:

```powershell
.\tools\codeql.ps1
```

This uses the CodeQL CLI to analyze JavaScript/TypeScript (UI), Python
(tests/tools) and C# (Windows Agent), writing SARIF files under `dist/codeql/`
and printing a per-language finding summary. The C/C++ firmware target needs a
full compile, so add `-IncludeCpp` after a PlatformIO build.

Install the CodeQL CLI first if it is missing: download the
`codeql-bundle-win64` release and extract it to `%USERPROFILE%\codeql`, or pass
`-CodeqlPath`. Use `-RamMb` if the query evaluator runs out of memory, and
`-FailOnFindings` to gate on security-relevant results. Open the SARIF files in
VS Code with the CodeQL extension, or read the JSON directly.
