
# RedLilyGO Setup

This guide covers first boot and optional host setup. Development commands are
in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Verify and flash

Build and run the repository's local checks before flashing:

```powershell
.\tools\verify.ps1
```

List devices, then flash the connected board:

```powershell
python -m platformio device list
.\tools\flash.ps1
```

For multiple devices, pass the current port shown by the list command:

```powershell
.\tools\flash.ps1 -Port COM7
```

Replace `COM7` with the port shown on your computer. Port names can change
when the board switches between bootloader and firmware.

## First boot

1. Open serial monitor at `115200` baud and confirm boot completes.
2. Join the `RedLilyGO` Wi-Fi network. The current firmware's default AP
   password is `redlilygo`.
3. Open `http://4.3.2.1/`.
4. Complete first-run setup with a username of 3 to 32 letters, numbers,
   underscores, or hyphens, and a password of 12 to 64 characters. This
   enables web Basic Authentication.
5. Change the AP password in Wi-Fi settings and reconnect using the new value.

The web interface uses HTTP in the default profile, without TLS. Keep the AP
private and do not forward its port to another network. Firmware updates use
USB flashing.

## Dashboard checks

After first boot, check the dashboard's device-reported status. If you want to
check the payload selector, copy a harmless test `.ds` file to the SD card and
remove it after the check. These steps test the connected build only; they do
not certify every peripheral or long-running behavior. A missing SD card,
Agent, microphone, or optional peripheral can be reported as unavailable.

For an isolated Evil Portal demonstration, copy
`payloads/evilap/redlilygo-demo.html` to `/Portal/` on the SD card. Select the
file in Files with `Use as Portal Template`, then use Marauder's `Start Evil
Portal` command. The portal AP name and password come from Settings ->
Evil Portal AP (`portal-ssid` / `portal-pwd`), or from the DuckyScript
launcher's `-n` / `-p` flags. The portal serves on `http://4.3.2.1/` (the same
address as the dashboard AP), so connected devices should show the OS sign-in
prompt; if not, browse there manually. The bundled page sends fixed dummy
values only. Do not enter real credentials.
Press BOOT three times to stop the portal and restore the dashboard AP.

## Board limitations

The `LILYGO-T-Dongle-S3` profile enables TFT, SD_MMC, button, status LED, Wi-Fi,
USB, and Marauder build support. Check `platformio.ini` and the connected board
revision before wiring optional modules; a compiled driver does not prove the
module is installed or working.
