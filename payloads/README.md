# DuckyScript payloads

DuckyScript `.ds` payloads and their guides. Requirements vary: some are
HID-only and type into a host, while others use RedLilyGO firmware features such
as the display, Marauder, serial, USB storage, USB-NCM, onboard buttons, or the
Windows Agent. Read the folder's guide before running a payload.

Folders with a dedicated guide:

| Example | Guide |
| --- | --- |
| Covert storage | [covertstorage](covertstorage/README.md) |
| Evil portal | [evilap](evilap/README.md) |
| Linux panic | [linux_panic](linux_panic/README.md) |
| Power off | [power_off](power_off/README.md) |
| Rickroll | [rickroll](rickroll/README.md) |
| USB Ethernet PCAP | [usb_ethernet_pcap](usb_ethernet_pcap/README.md) |
| VNC and screen sharing | [vnc](vnc/README.md) |
| Wi-Fi deauth and PMKID capture | [wifi_deauth_and_crypt_capture](wifi_deauth_and_crypt_capture/README.md) |

Additional folders without a separate guide:

- `malicious_ethernet_adapter/` - `NCMSetup.ds` and `cdrom.iso` for USB-NCM setup.
- `mount_whole_disk_and_run_command/` - `HIDDiskShell.ds` for USB storage plus host input.
- `wifi_connect_to_existing_ap/` - `WiFiJoin.ds` for Wi-Fi setup.
- `multiple_keyboard_layouts/` - `LayoutTour.ds` for keyboard-layout checks.
- `notepad_hello_world/` - `NotepadHello.ds`, a minimal HID payload.
- `system_info/` - `SystemInfo.ds`, `LocalRecon.ds`, and `WiFiDump.ds`.

Run these only on equipment and networks you own or are authorized to test.
