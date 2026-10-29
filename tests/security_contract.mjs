import fs from 'node:fs';
import assert from 'node:assert/strict';
import { webSource as web } from './lib/web_source.mjs';
const settings = fs.readFileSync('src/Utilities/Settings.cpp', 'utf8');
const cdc = fs.readFileSync('src/Devices/USB/USBCDC.cpp', 'utf8');
const agent = fs.readFileSync('src/Attacks/Agent/Agent.cpp', 'utf8');

assert.match(web, /static std::unordered_map<std::string, AuthFailureState> authFailures/);
assert.match(web, /authFailures\.size\(\) >= 32/);
assert.match(web, /ws\.handleHandshake\(isWebSocketAuthorized\)/);
assert.match(web, /audio\.handleHandshake\(isWebSocketAuthorized\)/);
assert.match(web, /Origin/);
assert.match(web, /url == "\/settings"/);
assert.match(web, /bool WebInternal::requestHasValidCsrf/);
assert.match(web, /bool WebInternal::requestNeedsMutationGuard/);
assert.match(web, /X-CSRF-Token/);
assert.match(web, /url == "\/csrf-token"/);
assert.match(web, /csrf_required/);
assert.match(web, /post_required/);
assert.match(web, /Remote client rejected: Agent is offline/);
const requestHandler = web.slice(web.indexOf('static void webRequestHandler'), web.indexOf('handleUpload(AsyncWebServerRequest'));
assert.match(requestHandler, /url == "\/uploadFile" && request->getResponse\(\) != nullptr\) return/,
  'final multipart callback must preserve the upload response');
assert.match(web, /client->close\(1013, "Agent unavailable"\)/);
assert.doesNotMatch(web, /ota-enabled|ElegantOTA|\/update/);
assert.match(settings, /wifi-pwd/);
assert.doesNotMatch(cdc, /Serial\.readBytes\(/, 'CDC input must not block the Arduino task');
assert.match(cdc, /now - frameStartedAt >= SERIAL_FRAME_TIMEOUT_MS/);
assert.match(cdc, /payloadLength > MAX_SERIAL_BUFFER_SIZE/);
assert.match(cdc, /lock\.unlock\(\);\s*if \(callback\)/);
assert.match(agent, /Agent heartbeat timed out/);
assert.match(agent, /USB mode changed/);

console.log('SECURITY_CONTRACT: pass');
