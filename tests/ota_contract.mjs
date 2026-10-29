import fs from 'node:fs';
import assert from 'node:assert/strict';
import { webSource as web } from './lib/web_source.mjs';
const flash = fs.readFileSync('tools/flash.ps1', 'utf8');
const smoke = fs.readFileSync('tools/hardware-smoke.ps1', 'utf8');

assert.doesNotMatch(web, /ElegantOTA|ota-enabled|\/update/);
const board = fs.readFileSync('src/Devices/Boards/LilyGO_TDongleS3/ESP32BoardSupport.cpp', 'utf8');
assert.doesNotMatch(board, /esp_ota|OTA image/);
assert.match(flash, /-t upload/);
assert.match(flash, /Board re-enumerated/);
assert.match(flash, /Get-BoardPort/);
assert.match(smoke, /\[switch\]\$Flash/);
assert.match(smoke, /\[string\]\$WebUri/);
assert.match(smoke, /Live WiFi setup and health smoke check passed/);
assert.match(smoke, /Board re-enumerated/);
assert.match(smoke, /Build-only smoke check passed/);

console.log('OTA_CONTRACT: pass');
