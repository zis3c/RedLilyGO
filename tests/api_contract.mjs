import assert from 'node:assert/strict';
import fs from 'node:fs';
import { webSource } from './lib/web_source.mjs';

// Firmware routes that are not modeled as API operations: captive-portal
// probes, embedded static assets, and legacy utility paths.
const utilityRoutes = new Set([
  '/', '/wpad.dat', '/favicon.ico',
  '/apple-touch-icon.png', '/apple-touch-icon-precomposed.png',
  '/assets/css/redlilygo.css',
  '/generate_204', '/gen_204', '/hotspot-detect.html', '/canonical.html',
  '/mobile/status.php', '/check_network_status.txt', '/hotspotdetect.html',
  '/library/test/success.html', '/ncsi.txt', '/connecttest.txt',
  '/success.txt', '/redirect',
]);

const codeRoutes = new Set();
for (const match of webSource.matchAll(/url == "([^"]+)"/g)) codeRoutes.add(match[1]);
for (const match of webSource.matchAll(/controlInterfaceWebServer\.on\("([^"]+)"/g)) codeRoutes.add(match[1]);
for (const match of webSource.matchAll(/AsyncCallbackJsonWebHandler\("([^"]+)"/g)) codeRoutes.add(match[1]);

const openapi = fs.readFileSync('openapi.yaml', 'utf8');
const openapiPaths = new Set([...openapi.matchAll(/^ {2}(\/\S+):/gm)].map((m) => m[1]));

const apiRoutes = [...codeRoutes].filter((route) => !utilityRoutes.has(route));
for (const route of apiRoutes) {
  assert.ok(openapiPaths.has(route), `openapi.yaml is missing firmware route ${route}`);
}
for (const path of openapiPaths) {
  assert.ok(apiRoutes.includes(path) || utilityRoutes.has(path),
    `openapi.yaml documents ${path}, but the firmware does not handle it`);
}

const socketPaths = [...webSource.matchAll(/AsyncWebSocket \w+\("([^"]+)"\)/g)].map((m) => m[1]);
const asyncapi = fs.readFileSync('asyncapi.yaml', 'utf8');
for (const path of socketPaths) {
  assert.ok(asyncapi.includes(`address: ${path}`), `asyncapi.yaml is missing channel ${path}`);
}

console.log('API_CONTRACT: pass');
