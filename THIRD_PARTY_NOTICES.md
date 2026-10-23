# Third-party notices

RedLilyGO is based on existing open-source projects and includes third-party
dependencies. Keep this file with source and binary releases.

## Main project credits

### USBArmyKnife

- Upstream: [i-am-shodan/USBArmyKnife](https://github.com/i-am-shodan/USBArmyKnife)
- License: MIT
- Copyright: i-am-shodan
- Relationship: RedLilyGO is based on and modifies this project.
- License text: [MIT](https://opensource.org/license/mit)

The upstream copyright and MIT permission notice remain applicable to the
pre-existing USBArmyKnife material. RedLilyGO copyright applies to original
changes and new material contributed to this repository.

### ESP32 Marauder

- Upstream: [justcallmekoko/ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder)
- License: MIT
- Copyright: Just Call Me Koko
- Local component: [lib/ESP32Marauder](./lib/ESP32Marauder)
- Component license: [lib/ESP32Marauder/LICENSE](./lib/ESP32Marauder/LICENSE)

The component license remains with the component when it is copied, modified,
or redistributed.

## Firmware dependencies

The direct dependency declarations are in [platformio.ini](./platformio.ini).
Licenses below were checked against the resolved PlatformIO packages used by
the local `LILYGO-T-Dongle-S3` verification build. Direct firmware dependencies
are pinned to versions, tags or Git commits; refresh this inventory when the
pins or resolved packages change.

| Component | Upstream | Resolved version | License |
| --- | --- | --- | --- |
| DuckScriptInterpreter | [i-am-shodan](https://github.com/i-am-shodan/DuckScriptInterpreter) | `23aa3ba` | MIT |
| ArduinoJson | [Benoit Blanchon](https://github.com/bblanchon/ArduinoJson) | `7.4.3` | MIT |
| LinkedList | [Ivan Seidel](https://github.com/ivanseidel/LinkedList) | `0.0.0-alpha+sha.dac3874d28` | MIT |
| AsyncTCPSock | [Yubox Node](https://github.com/yubox-node-org/AsyncTCPSock) | `1.0.2-dev` (archive tag `v1.0.3-dev`) | LGPL-3.0 |
| Adafruit TinyUSB Library | [i-am-shodan fork](https://github.com/i-am-shodan/Adafruit_TinyUSB_Arduino) | `0.0.0+sha.9305c9f` | MIT |
| ESPAsyncWebServer | [i-am-shodan fork](https://github.com/i-am-shodan/ESPAsyncWebServer) | `3.6.0` | LGPL-3.0 |
| backtrace-saver-esp32 | [b5i](https://github.com/b5i/backtrace-saver-esp32) | `2.0.0+sha.b8be0d1` | MIT |
| NimBLE-Arduino | [h2zero](https://github.com/h2zero/NimBLE-Arduino) | `1.4.3` | Apache-2.0 |
| OneButton | [Matthias Hertel](https://github.com/mathertel/OneButton) | `2.6.2` | BSD-3-Clause |
| LovyanGFX | [lovyan03](https://github.com/lovyan03/LovyanGFX) | `1.2.27` | MIT and BSD-2-Clause |
| MyLD2410 | [iavorvel](https://github.com/iavorvel/MyLD2410) | `1.0.13` | MIT |
| APA102 | [Pololu](https://github.com/pololu/apa102-arduino) | `3.0.0+sha.aa62193` | MIT |

`Uptime Library` was removed because its GPL-3.0 license was not a fit for the
project's MIT licensing. Uptime now uses ESP-IDF's `esp_timer_get_time()` API.
The build also uses the Espressif 32 platform, Arduino-ESP32 framework, and
ESP-IDF framework packages; keep their upstream license files with firmware
redistributions.

## Windows Agent dependencies

The direct package references are in [`tools/Agent/Agent.csproj`](./tools/Agent/Agent.csproj): Microsoft.Extensions.Logging (`8.0.1`, MIT), System.Drawing.Common (`8.0.0`, MIT), System.Security.Cryptography.ProtectedData (`8.0.0`, MIT), [ScreenCapture.NET.DX11](https://github.com/DarthAffe/ScreenCapture.NET) (`3.0.0`, LGPL-2.1-only), [SerialPortStream](https://github.com/jcurl/RJCP.DLL.SerialPortStream) (`2.4.2`, MS-PL), [Quamotion.RemoteViewing](https://github.com/quamotion/remoteviewing) (`1.1.211`, BSD-2-Clause), [SharpCompress](https://github.com/adamhathcock/sharpcompress) (`0.49.1`, MIT), System.Net.Http (`4.3.4`), System.Private.Uri (`4.3.2`), System.Text.RegularExpressions (`4.3.1`), [WebSocketSharp-netstandard](https://github.com/PingmanTools/websocket-sharp) (`1.0.1`, MIT), and [WmiLight](https://github.com/Readify/WmiLight) (`6.7.0`, MIT). Microsoft system packages use the license or license URL recorded in their NuGet metadata.

The currently resolved NuGet graph also includes LGPL-2.1-only packages (`HPPH` and `ScreenCapture.NET`), BSD-2-Clause (`Quamotion.RemoteViewing`), and MS-PL (`SerialPortStream` and `Validation`). NuGet transitive versions are not locked in the repository; review the resolved graph before each binary release.

## Bundled web assets

These vendored runtime assets ship with the dashboard or remote viewer. Their
license texts are kept beside the assets or in `ui/vnc/`.

| Asset | License | Local license text |
| --- | --- | --- |
| noVNC client core | MPL-2.0 | [`ui/vnc/LICENSE.MPL-2.0`](./ui/vnc/LICENSE.MPL-2.0) |
| noVNC HTML and CSS | BSD-2-Clause | [`ui/vnc/LICENSE.BSD-2-Clause`](./ui/vnc/LICENSE.BSD-2-Clause) |
| noVNC images | CC BY-SA 3.0 | [`ui/vnc/LICENSE.txt`](./ui/vnc/LICENSE.txt) |
| noVNC `pako` | MIT | [`ui/vnc/vendor/pako/LICENSE`](./ui/vnc/vendor/pako/LICENSE) |
| Ace editor | BSD-3-Clause | [`ui/content/assets/vendor/ace/LICENSE`](./ui/content/assets/vendor/ace/LICENSE) |
| Chart.js | MIT | [`ui/content/assets/vendor/chartjs/LICENSE.md`](./ui/content/assets/vendor/chartjs/LICENSE.md) |
| Leaflet | BSD-2-Clause | [`ui/content/assets/vendor/leaflet/LICENSE`](./ui/content/assets/vendor/leaflet/LICENSE) |
| Font Awesome Free | CC BY 4.0, SIL OFL 1.1, and MIT | [`ui/content/assets/vendor/fontawesome/LICENSE.txt`](./ui/content/assets/vendor/fontawesome/LICENSE.txt) |
| Tailwind CSS build tooling | MIT | Recorded in `ui/package-lock.json`; development dependency |

The `ui/package-lock.json` records license metadata for its 78 npm packages.
Those packages are build/test dependencies; generated UI assets are checked in.

## Project artwork and hardware references

The LilyGO reference images, diagrams, and 3D presentation model in `docs/`
were created by the project maintainer for RedLilyGO. They are original project
assets covered by the repository's MIT license in [`LICENSE`](./LICENSE).

## Redistribution

Before shipping firmware, source, an Agent package, or a hardware bundle:

1. Keep this file, `LICENSE`, and every bundled component license.
2. Add newly bundled license or NOTICE files to release archives.
3. Do not describe upstream code as authored by RedLilyGO or `zis3c`.
4. Review the exact resolved dependencies before commercial distribution.
