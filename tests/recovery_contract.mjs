import fs from 'node:fs';
import assert from 'node:assert/strict';

const board = fs.readFileSync('src/Devices/Boards/LilyGO_TDongleS3/ESP32BoardSupport.cpp', 'utf8');
const main = fs.readFileSync('src/main.cpp', 'utf8');

assert.match(board, /crash-streak/);
assert.match(board, /safeMode = crashStreak >= 3/);
assert.match(board, /millis\(\) >= 30000/);
assert.match(main, /isSafeMode\(\)/);
assert.match(main, /Marauder will start when requested from web UI/);

console.log('RECOVERY_CONTRACT: pass');
