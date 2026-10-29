import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { webSource as webServer } from './lib/web_source.mjs';

const index = await readFile('ui/content/index.html', 'utf8');
const remote = await readFile('ui/vnc/index.html', 'utf8');
const agentStatus = await readFile('ui/src/features/tools/agent-status.js', 'utf8');
const toolsHtml = await readFile('ui/src/features/tools/tools.html', 'utf8');
const marauderJs = await readFile('ui/src/features/tools/marauder.js', 'utf8');
const filesHtml = await readFile('ui/src/features/files/files.html', 'utf8');
const payloads = await readFile('ui/src/features/tools/payloads.js', 'utf8');
const sidebar = await readFile('ui/src/layout/sidebar.html', 'utf8');
const build = await readFile('build_ui.py', 'utf8');
const logging = await readFile('src/Diagnostics/Logging.cpp', 'utf8');
const marauder = await readFile('src/Attacks/Marauder/Marauder.cpp', 'utf8');
const wifi = await readFile('src/Devices/WiFi/WiFiDevice.cpp', 'utf8');
const uiScript = await readFile('ui/content/assets/js/ui.js', 'utf8');

assert.match(remote, /<meta charset="UTF-8">/);
assert.match(remote, /const rfbAsset = window\.location\.pathname === '\/remote'[\s\S]*'\/remote\/core\/rfb\.js'/);
assert.match(remote, /<div id="status">Starting<\/div>/);
assert.doesNotMatch(remote, /<div id="status">Loading<\/div>/);

for (const requiredId of ['tab-1', 'tab-4', 'tab-8', 'agentStatus']) {
  assert.match(index, new RegExp(`id="${requiredId}"`), `missing UI contract: ${requiredId}`);
}

for (const requiredId of ['reconnectButton', 'keyboardButton', 'enterButton', 'escapeButton', 'tabButton', 'backspaceButton', 'ctrlButton', 'altButton', 'sendCtrlAltDelButton', 'lockButton', 'leftClickButton', 'rightClickButton', 'middleClickButton', 'scrollUpButton', 'scrollDownButton']) {
  assert.match(remote, new RegExp(`id="${requiredId}"`), `missing remote control: ${requiredId}`);
}

const remoteIds = new Set([...remote.matchAll(/\bid=["']([^"']+)/g)].map(match => match[1]));
const remoteReferences = [...remote.matchAll(/getElementById\(["']([^"']+)/g)].map(match => match[1]);
for (const referencedId of new Set(remoteReferences)) {
  assert.ok(remoteIds.has(referencedId), `remote JS references missing DOM id: ${referencedId}`);
}

for (const remoteBehavior of [
  'inputmode="text"',
  'addEventListener(\'input\', handleMobileText)',
  'trackpad-indicator',
  'sendCtrlAltDel',
  'sendMouseButton',
  'sendMouseWheel',
  'scheduleReconnect',
  'connectRemote',
  'Reconnecting in',
  'clearTimeout(reconnectTimer)',
  "readQueryVariable('path', 'websockify')",
]) {
  assert.ok(remote.includes(remoteBehavior), `missing remote behavior: ${remoteBehavior}`);
}

// codeql[js/incomplete-multi-character-sanitization]
const visibleRemoteText = remote
  .replace(/<!--[\s\S]*?-->/g, '')
  // codeql[js/bad-tag-filter]
  .replace(/<script\b[\s\S]*?<\/script\s*>/gi, '')
  .replace(/<style\b[\s\S]*?<\/style\s*>/gi, '');
assert.doesNotMatch(visibleRemoteText, />[^<]*VNC[^<]*</i, 'remote UI exposes vendor terminology');

assert.match(index, /Dashboard/);
assert.match(index, /Remote/);
assert.match(index, /id="agentVersion"/);
assert.match(index, /id="agentCompatibility"/);
assert.doesNotMatch(index, /id="tab-5"|id="tab-9"|id="tab-11"/);
assert.doesNotMatch(sidebar, /data-tab="tab-5"|data-tab="tab-9"|data-tab="tab-11"/);
assert.match(toolsHtml, /value="sniffraw">WiFi packet capture \(PCAP\)/);
assert.match(toolsHtml, /value="stopscan"/);
assert.match(toolsHtml, /value="evilportal -c start"/);
assert.match(toolsHtml, /marauderPortalTemplate/);
assert.match(filesHtml, /filePortalButton/);
assert.match(payloads, /\/evilportal\/files/);
assert.match(payloads, /redlily_portal_template/);
assert.match(marauderJs, /\/evilportal\?action=start/);
assert.doesNotMatch(build, /features.*(?:portal|captures)/);
assert.match(agentStatus, /agentVersion/);
assert.match(agentStatus, /agentProtocolCompatible/);
assert.doesNotMatch(uiScript, /otaEnabled|otaToggle|otaUpdateButton/);
assert.match(build, /CONTENT_DIR = os\.path\.join\(REPO_ROOT, "ui", "content"\)/);
assert.match(build, /"assets", "js", "ui\.js"/);
assert.match(build, /"assets", "css", "redlilygo\.css"/);
for (const header of ['X-Content-Type-Options', 'X-Frame-Options', 'Referrer-Policy']) {
  assert.match(webServer, new RegExp(header), `missing security header: ${header}`);
}
assert.match(webServer, /handleHandshake\(isWebSocketAuthorized\)/);
assert.match(webServer, /agentVersion/);
assert.match(webServer, /agentVersion\(\)/);
assert.match(webServer, /url == "\/remote"/);
assert.doesNotMatch(webServer, /url == "\/remote\/"/);
assert.match(webServer, /agentProtocolCompatible/);
const settings = await readFile('src/Utilities/Settings.cpp', 'utf8');
assert.match(settings, /isSensitiveSetting/);
assert.match(settings, /wifi-pwd/);
assert.match(settings, /isSensitiveSetting\(settingName\).*\"\*\*\*\*\*\*\*\*\"/s);
assert.doesNotMatch(wifi, /ota-enabled/);
assert.doesNotMatch(webServer, /otaEnabled|ElegantOTA|\/update/);
assert.match(webServer, /marauder_radio_queued/);
assert.match(webServer, /activeMode/);
assert.match(marauder, /RedLilyGO-Portal/);
assert.match(marauder, /setPortalSsid/);
assert.match(index, /retryEmbeddedAsset/);
assert.match(index, /loadingerror/);
assert.match(uiScript, /Marauder radio command/);
assert.match(uiScript, /compactLogLine/);
assert.match(logging, /timestamp \+ " " \+ shortComponent\(component\)/);
assert.match(logging, /return "AP \+ PW"/);
assert.match(webServer, /return request != nullptr/);
assert.match(uiScript, /agentCompatibility/);
assert.match(uiScript, /ac\.textContent = 'Unknown'/);
assert.match(remote, /Remote connection lost/);
assert.match(remote, /Reconnecting in/);
assert.match(remote, /sendMouseButton/);
assert.match(remote, /sendMouseWheel/);
assert.match(remote, /function sendCtrlAltDel\(\) \{[\s\S]*if \(!rfb\)[\s\S]*Remote connection unavailable/);
console.log('UI_CONTRACT: pass');
