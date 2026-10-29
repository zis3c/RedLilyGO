# Example: USB Ethernet PCAP

Captures USB NCM network traffic into PCAP. Capture only an interface and network you are authorized to inspect. PCAP can contain credentials and private data.

## Requirements

- Board and firmware profile with USB NCM.
- Host OS with CDC-NCM support.
- SD card with free space.

## Setup and usage

1. Connect over WiFi and set `usbDeviceType` to `NCM` in **Settings**.
2. Reconnect USB so the host enumerates the new interface.
3. Copy [NCMCapture.ds](NCMCapture.ds)
   to `/Scripts/` and run it from the payload list.
4. Wait for `PCAP Stopped`, safely eject the card, and find
   `/Captures/Other/usbncm_0.pcap`.

If NCM is not visible, check cable, host drivers, USB mode, and re-enumeration. Do not remove the card while capture is active.
