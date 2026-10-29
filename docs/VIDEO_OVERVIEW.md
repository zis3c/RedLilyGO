# Feature Tutorials & Video Walkthroughs

Hands-on walkthroughs and demonstration videos for core RedLilyGO firmware workflows.

All video clips are stored locally under [`docs/videos/`](./videos/) in web-optimized H.264 MP4 format:

| # | Feature | Walkthrough | Duration | Size |
| --- | --- | --- | --- | --- | --- |
| **01** | First Connection & Web Console | [Tutorial 1](#tutorial-1-first-connection--web-console-navigation) · [Watch](./videos/01_connect_wifi_and_web_console.mp4) | 41s | 3.95 MB |
| **02** | System Settings & Security | [Tutorial 2](#tutorial-2-system-settings--security-configuration) · [Watch](./videos/02_system_settings_configuration.mp4) | 24s | 1.69 MB |
| **03** | Native USB HID Remote Input | [Tutorial 3](#tutorial-3-native-usb-hid-remote-input-control) · [Watch](./videos/03_native_usb_hid_remote_control.mp4) | 19s | 0.55 MB |
| **04** | DuckyScript Payload Automation | [Tutorial 4](#tutorial-4-duckyscript-payload-automation) · [Watch](./videos/04_duckyscript_payload_execution.mp4) | 22s | 1.41 MB |
| **05** | ESP32 Marauder Wi-Fi SSID Spam | [Tutorial 5](#tutorial-5-esp32-marauder-wi-fi-random-ssid-spam) · [Watch](./videos/05_wifi_marauder_beacon_ssid_spam.mp4) | 45s | 4.53 MB |
| **06** | EvilAP / Captive Portal Lab | [Tutorial 6](#tutorial-6-evilap--captive-portal-demonstration) · [Watch](./videos/06_evil_portal_demonstration.mp4) | 95s | 8.51 MB |

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
3. Open a browser and navigate to `http://4.3.2.1/`.
4. Review the dashboard runtime metrics including SD storage, heap memory, internal temperature, and system status.

#### Video Demonstration

Watch: [01_connect_wifi_and_web_console.mp4 (41s, 3.95 MB)](./videos/01_connect_wifi_and_web_console.mp4)

---

### Tutorial 2: System Settings & Security Configuration

**Goal**: Configure Wi-Fi parameters, set up Web Basic Authentication, and adjust display settings.

#### Prerequisites

- Connected to the web dashboard (`http://4.3.2.1/`).

#### Steps

1. Click the **Settings** tab in the navigation bar.
2. Update **Wi-Fi Settings** (AP SSID, passphrase, and channel).
3. Set up **Web Authentication** with a username (3-32 characters) and password (12-64 characters).
4. Adjust **Hardware Settings** (TFT timeout, brightness, and status LED toggles).
5. Click **Save Settings** to apply changes.

#### Video Demonstration

Watch: [02_system_settings_configuration.mp4 (24s, 1.69 MB)](./videos/02_system_settings_configuration.mp4)

---

### Tutorial 3: Native USB HID Remote Input Control

**Goal**: Control a target machine's mouse and keyboard directly from a phone or browser without installing host-side software.

#### Prerequisites

- LILYGO T-Dongle-S3 plugged into the target host via USB.
- USB mode set to `Serial + HID`.
- Phone or tablet connected to the RedLilyGO AP.

#### Steps

1. Navigate to `http://4.3.2.1/remote/input`.
2. Use the **Virtual Touchpad** by dragging to move the cursor, tapping for left-click, or tapping right/middle buttons.
3. Use the **Keyboard Input** text field or shortcut buttons (`Enter`, `GUI`, `Ctrl+C`, `Esc`) to inject keystrokes into the target.

#### Video Demonstration

Watch: [03_native_usb_hid_remote_control.mp4 (19s, 0.55 MB)](./videos/03_native_usb_hid_remote_control.mp4)

---

### Tutorial 4: DuckyScript Payload Automation

**Goal**: Select and trigger keystroke automation payloads stored on the MicroSD card.

#### Prerequisites

- MicroSD card inserted with `.ds` scripts inside `/Scripts`.
- Dongle connected via USB in HID mode.

#### Steps

1. Open the **Ducky / Automation** tab.
2. Select a `.ds` payload script from the dropdown menu.
3. Review payload lines and options.
4. Click **Inject Payload**.
5. Monitor execution logs in the web interface as keystrokes are sent.

#### Video Demonstration

Watch: [04_duckyscript_payload_execution.mp4 (22s, 1.41 MB)](./videos/04_duckyscript_payload_execution.mp4)

---

### Tutorial 5: ESP32 Marauder Wi-Fi Random SSID Spam

**Goal**: Run a beacon flood assessment in an authorized test environment.

#### Prerequisites

- Authorized lab environment only.

#### Steps

1. Open the **Marauder** page from the navigation bar.
2. Select **Random SSID Spam** from the command menu.
3. Click **Start Scan/Attack** to broadcast synthetic beacon frames.
4. Note that channel-hopping attacks will temporarily disconnect the web console while running.
5. Stop the attack by clicking **Stop active scan/attack** in the web UI, or physically triple-clicking the BOOT button on the dongle.

#### Video Demonstration

Watch: [05_wifi_marauder_beacon_ssid_spam.mp4 (45s, 4.53 MB)](./videos/05_wifi_marauder_beacon_ssid_spam.mp4)

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
4. Connect a test client to the portal SSID to trigger the captive login prompt.
5. Submit dummy test credentials.
6. Review recorded entries under `/Captures/Portal/evilportal_submissions.jsonl`.
7. Stop the attack via the web UI or triple-click the BOOT button.

#### Video Demonstration

Watch: [06_evil_portal_demonstration.mp4 (95s, 8.51 MB)](./videos/06_evil_portal_demonstration.mp4)

---

## Project Context & Additional Features

RedLilyGO is an open-source cybersecurity and hardware testing project.

The firmware includes several other functional capabilities that are not demonstrated in video clips:

- **BLE Pentest**: Sniffing, BLE advertisement spam, and device scanning.
- **Windows Host Agent**: USB CDC serial bridge and host integration tools.
- **Remote Display**: Screen streaming via `/remote/display`.
- **USB-NCM Capture**: Raw network interface packet capture.

These features remain fully functional in code and are documented in [FIRMWARE_REFERENCE.md](./FIRMWARE_REFERENCE.md).

---

## Hardware Compatibility Testing

Device verification was conducted across a diverse range of client hardware and operating systems:

| Device | Category | OS | Verified Workflows |
| --- | --- | --- | --- |
| **Huawei MateBook D14** | Laptop / Host PC | Windows 11 | Flashing, USB CDC serial, USB HID emulation, Agent integration. |
| **Samsung Galaxy A30** | Smartphone | Android 10 | AP connection, mobile web interface, Ducky injection, Marauder controls. |
| **iPhone XS** | Smartphone | iOS 17 | AP connection, captive portal trigger, mobile Safari layout. |
| **Samsung Galaxy Tab A8** | Tablet | Android 13 | Tablet web console, file manager, telemetry charts, touchpad input. |

All four devices verified SoftAP connection and responsive web console controls without drops.
