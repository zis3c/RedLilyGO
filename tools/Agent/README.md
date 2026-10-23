# RedLilyGO Agent

Optional Windows companion for USB serial transport, VNC screen sharing, Remote
input, and explicitly requested host commands. It is not required for normal
firmware operation.

> Run only against a host you own or are explicitly authorised to control.

## Start

With the LilyGO board connected by USB:

```powershell
dotnet run --project .\tools\Agent\Agent.csproj
```

Build a standalone Windows package that does not require .NET to be installed:

```powershell
.\tools\publish-agent.ps1
```

Run `dist\agent-win-x64\RedLilyGO.Agent.exe` from the generated package.

Check the package without opening a serial connection:

```powershell
dist\agent-win-x64\RedLilyGO.Agent.exe --help
dist\agent-win-x64\RedLilyGO.Agent.exe --version
```

The Agent auto-detects the default RedLilyGO USB identity `CAFE:403F`.

For custom firmware identities:

```powershell
dotnet run --project .\tools\Agent\Agent.csproj -- vid=1234 pid=5678
```

Keep this terminal running while using Remote Control from the RedLilyGO web interface.

The Agent shows connection lifecycle messages in the console and writes a local
diagnostic log to `%LOCALAPPDATA%\RedLilyGO\agent.log`. The log stays on the
local machine. Press `Ctrl+C` for a clean shutdown.

The Agent reports its version during the existing USB status handshake so the
web UI can show Agent and firmware compatibility information.

From the repository root, run `.\tools\verify.ps1` to validate the UI contract,
UI syntax, Agent build, and firmware build together.

The board can re-enumerate between USB and application modes. The Agent retries
until the matching USB identity appears, so a fixed COM port is not required.
Use the version and protocol status in the web UI to confirm compatibility.

## Web protection

HTTP Basic Authentication is enabled by first-run setup and can be disabled in
Wi-Fi settings. The web interface uses HTTP in the default profile, so Basic
Authentication does not encrypt credentials in transit. Change the default AP
password, keep the device network private, and do not expose the web service to
an untrusted network. The USB VID/PID check locates the Agent device; it is not
cryptographic authentication. Firmware updates use USB flashing.
