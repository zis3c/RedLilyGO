import assert from 'node:assert/strict';
import fs from 'node:fs';

// Guards the structural improvements: the HTTP layer must stay decomposed and
// the display layer must stay split.
const webDir = 'src/Comms/Web';
const webFiles = fs.readdirSync(webDir).filter((name) => name.endsWith('.cpp'));
assert.ok(webFiles.length >= 5, `expected split web modules, found ${webFiles.length}`);

for (const required of ['WebServer.cpp', 'WebAuth.cpp', 'WebResponse.cpp', 'WebStatic.cpp', 'WebUpload.cpp', 'WebWebSocket.cpp']) {
  assert.ok(webFiles.includes(required), `missing web module: ${required}`);
}

const serverLines = fs.readFileSync(`${webDir}/WebServer.cpp`, 'utf8').split('\n').length;
assert.ok(serverLines <= 1100, `WebServer.cpp regressed to ${serverLines} lines`);

const displayDir = 'src/Devices/Display';
const displayFiles = fs.readdirSync(displayDir).filter((name) => name.endsWith('.cpp'));
for (const required of ['DisplayDevice.cpp', 'DisplayScreens.cpp']) {
  assert.ok(displayFiles.includes(required), `missing display module: ${required}`);
}
const displayLines = fs.readFileSync(`${displayDir}/DisplayDevice.cpp`, 'utf8').split('\n').length;
assert.ok(displayLines <= 650, `DisplayDevice.cpp regressed to ${displayLines} lines`);

console.log('ARCHITECTURE_CONTRACT: pass');
