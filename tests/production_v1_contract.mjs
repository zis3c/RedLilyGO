import assert from 'node:assert/strict';
import fs from 'node:fs';
import { webSource as web } from './lib/web_source.mjs';
const marauder = fs.readFileSync('src/Attacks/Marauder/Marauder.cpp', 'utf8');
const portal = fs.readFileSync('lib/ESP32Marauder/esp32_marauder/EvilPortal.cpp', 'utf8');
const buffer = fs.readFileSync('lib/ESP32Marauder/esp32_marauder/Buffer.cpp', 'utf8');
const html = fs.readFileSync('ui/content/index.html', 'utf8');
const ui = fs.readFileSync('ui/content/assets/js/ui.js', 'utf8');
const smoke = fs.readFileSync('tools/hardware-smoke.ps1', 'utf8');
const packageScript = fs.readFileSync('tools/package-release.ps1', 'utf8');

for (const pattern of [
  /\/setup\/status/,
  /url == "\/health"/,
  /Content-Security-Policy/,
  /MAX_UPLOAD_BYTES/,
  /validStoragePath/,
  /usbHidReady/,
  /usbMounted/,
  /screensaver/,
  /marauder_radio_queued/,
  /replaceExisting/
]) assert.match(web, pattern);
assert.match(web, /esp_timer_get_time\(\)/);
assert.doesNotMatch(fs.readFileSync('platformio.ini', 'utf8'), /Uptime-Library/);

assert.match(html, /id="setupGate"/);
assert.match(ui, /requestWithRetry/);
assert.match(ui, /requestWithRetry/);
assert.doesNotMatch(ui, /PAYLOAD_LIBRARY|payload\.bat|SIMULATED|Math\.random/);
assert.doesNotMatch(ui, /populatePayloadLibrary|payloadLibrarySelect/);
assert.doesNotMatch(ui, /temp\.ds/);
assert.match(ui, /rawCommandInput/);
assert.match(ui, /Marauder radio command/);
assert.match(html, /retryEmbeddedAsset/);
assert.match(html, /loadingerror/);
assert.match(marauder, /RedLilyGO-Portal/);
assert.match(marauder, /setPortalSsid/);
assert.match(ui, /screensaverToggle/);
assert.match(ui, /\/download\?filename=/);
assert.match(ui, /fetch\('\/wifi\/scan'\)/);
assert.match(web, /url == "\/capture"/);
assert.match(portal, /HTTP_POST/);
assert.match(portal, /submissionHistory/);
assert.match(buffer, /void Buffer::close\(\)/);
assert.match(marauder, /marauderRadioActive = portalActive/);
assert.match(marauder, /wifiCaptureActive/);
assert.doesNotMatch(html, /value="0x0001"/);
assert.match(ui, /currentUsbDeviceType/);
assert.match(smoke, /CAFE:403F/);
assert.match(smoke, /hardware-evidence/);
assert.match(packageScript, /release-manifest\.json/);
assert.match(packageScript, /SHA256SUMS\.txt/);

console.log('PRODUCTION_V1_CONTRACT: pass');
