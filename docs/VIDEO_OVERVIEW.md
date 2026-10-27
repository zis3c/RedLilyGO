# Feature Tutorials & Video Walkthroughs

Hands-on walkthroughs and demonstration videos for core RedLilyGO firmware workflows.

All video clips are stored locally under [`docs/videos/`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/) in web-optimized H.264 MP4 format:

| # | Feature | Video Clip | Size |
|---|---|---|---|
| **01** | First Connection & Web Console | [`01_connect_wifi_and_web_console.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/01_connect_wifi_and_web_console.mp4) | 3.95 MB |
| **02** | System Settings & Security | [`02_system_settings_configuration.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/02_system_settings_configuration.mp4) | 1.69 MB |
| **03** | Native USB HID Remote Input | [`03_native_usb_hid_remote_control.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/03_native_usb_hid_remote_control.mp4) | 0.55 MB |
| **04** | DuckyScript Payload Automation | [`04_duckyscript_payload_execution.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/04_duckyscript_payload_execution.mp4) | 1.41 MB |
| **05** | ESP32 Marauder Wi-Fi SSID Spam | [`05_wifi_marauder_beacon_ssid_spam.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/05_wifi_marauder_beacon_ssid_spam.mp4) | 4.53 MB |
| **06** | EvilAP / Captive Portal Lab | [`06_evil_portal_demonstration.mp4`](file:///c:/Users/RADZ/project%20io/RedLilyGO/docs/videos/06_evil_portal_demonstration.mp4) | 8.51 MB |

---

## Step-by-Step Tutorials

### Tutorial 1: First Connection & Web Console Navigation

**Goal**: Connect to the SoftAP Wi-Fi network and open the dashboard.

#### Prerequisites
- LILYGO T-Dongle-S3 plugged into USB power.
- Client device (laptop, phone, or tablet) with Wi-Fi.

#### Steps
1. Search for nearby Wi-Fi networks and connect to **`RedLilyGO`**.
2. Enter the default AP password: `redlilygo`.
3. Open a browser and navigate to:
   ```text
   http://4.3.2.1/
   ```
4. The dashboard displays the following runtime metrics:
   - **SD Storage**: Total and free MicroSD space.
   - **Heap Memory**: Free dynamic RAM.
   - **Internal Temperature**: ESP32-S3 internal sensor reading.
   - **System Status & Uptime**: Active profile and hardware capabilities.

#### Video Demonstration
Watch: [01_connect_wifi_and_web_console.mp4](./videos/01_connect_wifi_and_web_console.mp4)

---

### Tutorial 2: System Settings & Security Configuration

**Goal**: Configure Wi-Fi parameters, set up Web Basic Authentication, and adjust display settings.

#### Prerequisites
- Connected to the web dashboard (`http://4.3.2.1/`).

#### Steps
1. Click the **Settings** tab in the navigation bar.
2. **Wi-Fi Settings**:
   - Update AP SSID, passphrase, and channel.
3. **Web Authentication**:
   - Set up a username (3–32 characters) and password (12–64 characters) to enable HTTP Basic Authentication.
4. **Hardware Settings**:
   - Adjust TFT timeout, brightness, and status LED toggles.
5. Click **Save Settings** to apply.

#### Video Demonstration
Watch: [02_system_settings_configuration.mp4](./videos/02_system_settings_configuration.mp4)

---

### Tutorial 3: Native USB HID Remote Input Control

**Goal**: Control a target machine's mouse and keyboard directly from a phone or browser without installing host-side software.

#### Prerequisites
- LILYGO T-Dongle-S3 plugged into the target host via USB.
- USB mode set to `Serial + HID`.
- Phone or tablet connected to the RedLilyGO AP.

#### Steps
1. Navigate to:
   ```text
   http://4.3.2.1/remote/input
   ```
2. **Virtual Touchpad**:
   - Drag to move the mouse cursor.
   - Tap for Left Click; use on-screen buttons for Right and Middle Click.
3. **Keyboard Input**:
   - Use the text input field or shortcut buttons (`Enter`, `GUI`, `Ctrl+C`, `Esc`) to inject keystrokes into the target.

#### Video Demonstration
Watch: [03_native_usb_hid_remote_control.mp4](./videos/03_native_usb_hid_remote_control.mp4)

---

### Tutorial 4: DuckyScript Payload Automation

**Goal**: Select and trigger keystroke automation payloads stored on the MicroSD card.

#### Prerequisites
- MicroSD card inserted with `.ds` scripts inside `/Scripts`.
- Dongle connected via USB in HID mode.

#### Steps
1. Open the **Ducky / Automation** tab.
2. Select a `.ds` payload script from the dropdown.
3. Review payload lines and options.
4. Click **Inject Payload**.
5. The web interface displays execution logs as keystrokes are sent.

#### Video Demonstration
Watch: [04_duckyscript_payload_execution.mp4](./videos/04_duckyscript_payload_execution.mp4)

---

### Tutorial 5: ESP32 Marauder Wi-Fi Random SSID Spam

**Goal**: Run a beacon flood assessment in an authorized test environment.

#### Prerequisites
- Authorized lab environment only.

#### Steps
1. Open the **Marauder** page from the navigation bar.
2. Select **Random SSID Spam** from the command menu.
3. Click **Start Scan/Attack**.
4. The radio broadcasts synthetic beacon frames across channels.
5. > [!NOTE]
   > Because the ESP32-S3 has one Wi-Fi radio, channel-hopping attacks will temporarily disconnect the web console.
6. **Stopping the attack**:
   - Click **Stop active scan/attack** in the web UI, or
   - Physically **triple-click the BOOT button** on the dongle to cancel radio attacks and restore the SoftAP immediately.

#### Video Demonstration
Watch: [05_wifi_marauder_beacon_ssid_spam.mp4](./videos/05_wifi_marauder_beacon_ssid_spam.mp4)

---

### Tutorial 6: EvilAP / Captive Portal Demonstration

**Goal**: Deploy a captive portal template in an isolated lab to test rogue AP handling.

#### Prerequisites
- HTML template saved under `/Portal` on the SD card (e.g., `redlilygo-demo.html`).
- Use synthetic test credentials only.

#### Steps
1. Open the **Files** tab and navigate to `/Portal`.
2. Choose a template and click **Use as Portal Template**.
3. Open the **Marauder** tab and select **Start Evil Portal**.
4. Connect a test client to the portal SSID; the captive login prompt will appear.
5. Submit dummy test credentials.
6. Review recorded entries under `/Captures/Portal/evilportal_submissions.jsonl`.
7. Stop the attack via the web UI or triple-click the BOOT button.

#### Video Demonstration
Watch: [06_evil_portal_demonstration.mp4](./videos/06_evil_portal_demonstration.mp4)

---

## Project Context & Additional Features

RedLilyGO is a personal side project (*"projek suka-suka"*).

The firmware includes several other functional capabilities that are not shown on video:
- **BLE Pentest**: Sniffing, BLE advertisement spam, and device scanning.
- **Windows Host Agent**: USB CDC serial bridge and host integration tools.
- **Remote Display**: Screen streaming via `/remote/display`.
- **USB-NCM Capture**: Raw network interface packet capture.

Video walkthroughs for these were skipped due to university assignments and my degree Final Year Project (FYP) workload. These features remain functional in code and are documented in [FIRMWARE_REFERENCE.md](./FIRMWARE_REFERENCE.md).

---

## Hardware Compatibility Testing

Device verification was conducted on personal hardware on hand during testing (*"kekurangan peranti semasa sesi ujian"*):

| Device | Category | OS | Verified Workflows |
|---|---|---|---|
| **Huawei MateBook D14** | Laptop / Host PC | Windows 11 | Flashing, USB CDC serial, USB HID emulation, Agent integration. |
| **Samsung Galaxy A30** | Smartphone | Android 10 | AP connection, mobile web interface, Ducky injection, Marauder controls. |
| **iPhone XS** | Smartphone | iOS 17 | AP connection, captive portal trigger, mobile Safari layout. |
| **Perantisiswa A8 (Samsung Galaxy Tab A8)** | Tablet | Android 13 | Tablet web console, file manager, telemetry charts, touchpad input. |

All four devices verified SoftAP connection and responsive web console controls without drops.
