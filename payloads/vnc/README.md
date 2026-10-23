# Example: VNC and screen sharing

The optional RedLilyGO Agent exposes the host display over the board's USB CDC
connection. The USB VID/PID identifies the device; it is not cryptographic
authentication. Use only on a host and display you own or administer.

## Requirements

- Supported board connected by USB CDC serial.
- Agent from `tools/Agent` on a Windows host with .NET 8.
- Capturable host display and browser connected to device web UI.

## Setup and usage

1. Build or publish the Agent using `tools/Agent/README.md`.
2. Start it and connect the board.
3. Confirm the Agent reports a connected device.
4. Open the web interface and select **VNC**.
5. Wait for the connection status and a host screen frame.

If the connection does not start, check that the Agent is running on the USB-
connected host, the Agent sees the configured device identity, no other program
has opened the serial port, and Windows exposes a capturable display. Re-run
`python -m platformio device list` after USB re-enumeration. Stop the Agent
before unplugging.

## When the Agent is offline

VNC cannot start until `RedLilyGO.Agent.exe` is running on the PC that is USB-connected to the board. Start it manually with `.\tools\start-agent.ps1`, or use the automated payload below. If you open the VNC page while the Agent is offline, the page shows a notice with the command to run and reconnects on its own once it detects the Agent.

USB HID Control at `/remote/input` does not use the Agent, so it is unaffected by this state.

## Automated start payload

[`AgentVNC.ds`](AgentVNC.ds) switches the board to HID + storage, then types a
PowerShell command that checks a fixed set of drive letters for
`RedLilyGO.Agent.exe` and starts it hidden. Copy the published
`RedLilyGO.Agent.exe` to the SD card root first. Keep the typed command short:
Windows Run truncates long commands near 260 characters. Keep the target PC
unlocked and focused while the payload runs. Run only on a host you own or are
authorized to test.
