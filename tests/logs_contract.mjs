import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { webSource as web } from './lib/web_source.mjs';

const filter = await readFile('ui/src/features/logs/filter.js', 'utf8');
const saved = await readFile('ui/src/features/logs/saved.js', 'utf8');
const mini = await readFile('ui/src/features/tools/marauder.js', 'utf8');
const logging = await readFile('src/Diagnostics/Logging.cpp', 'utf8');
const ble = await readFile('lib/ESP32Marauder/esp32_marauder/WiFiScan.cpp', 'utf8');
const telemetry = await readFile('ui/src/features/dashboard/telemetry.js', 'utf8');

assert.match(filter, /line\.replace\(\/\^\(\\d\{2\}:\\d\{2\}\)\\s\+\\1\\s\+\//);
assert.match(filter, /const compact = line\.match/);
assert.match(filter, /if \(!message\) return ''/);
assert.match(saved, /logBox\.value = text \|\| 'No saved SD logs'/);
assert.doesNotMatch(saved, /indexOf\('\> '\)/);
assert.match(mini, /function formatMiniLogLines\(values\)/);
assert.match(mini, /line = line\.replace\(\/\^>\\s\*\//);
assert.match(mini, /formatMiniLogLines\(\[label \+ ': ' \+ command, status\]\)/);
assert.match(logging, /message == "Marauder command queued"/);
assert.match(logging, /message == "Running Marauder command"/);
assert.match(logging, /message == "Marauder command finished"/);
assert.doesNotMatch(web, /Running Marauder command from raw input/);
assert.doesNotMatch(web, /Run Marauder command from web UI/);
assert.match(web, /Marauder command queued/);
assert.match(ble, /static bool prepareBleAdvertiser\(\)/);
assert.match(ble, /if \(!prepareBleAdvertiser\(\)\)/);
assert.match(ble, /BLE advertise failed/);

for (const name of ['executeSourApple', 'executeSwiftpairSpam']) {
    const start = ble.indexOf(`void WiFiScan::${name}`);
    const end = ble.indexOf('\n}', start);
    const body = ble.slice(start, end);
    assert.doesNotMatch(body, /NimBLEDevice::(init|deinit)|createServer/);
}

assert.match(telemetry, /'text-white text-\[13px\] font-medium'/);
console.log('LOGS_CONTRACT: pass');
