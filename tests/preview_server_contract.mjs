import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const preview = await readFile('preview_server.py', 'utf8');

assert.match(preview, /REDLILYGO_PREVIEW_PORT/);
assert.match(preview, /parsed_path in \('\/remote', '\/remote\/input'\)/);
assert.match(preview, /parsed_path\.startswith\('\/remote\/'\)[\s\S]*parsed_path\[8:\]/);
assert.match(preview, /DIRECTORY_VNC, 'index\.html'/);
assert.match(preview, /def safe_path\(root, relative_path\)/);
assert.match(preview, /commonpath\(\(normalized_root, normalized_candidate\)\)/);
assert.match(preview, /custom 404 page/);
for (const endpoint of ['/setup/status', '/data.json', '/health', '/wifi/scan', '/evilportal', '/deauth']) {
  assert.match(preview, new RegExp(`'${endpoint.replace('/', '\\/')}'`), `missing preview fixture: ${endpoint}`);
}
assert.match(preview, /def do_GET\(self\)/);
assert.match(preview, /Cache-Control', 'no-store'/);

console.log('PREVIEW_SERVER_CONTRACT: pass');
