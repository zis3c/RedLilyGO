import assert from 'node:assert/strict';
import fs from 'node:fs';
import { webSource } from './lib/web_source.mjs';

// Guards the POST + per-boot X-CSRF-Token mutation policy. Every state-changing
// route must be listed by the mutation guard, served over POST, and gated by the
// CSRF token; the read routes must remain read-only.
const guardStart = webSource.indexOf('bool WebInternal::requestNeedsMutationGuard');
assert.ok(guardStart >= 0, 'requestNeedsMutationGuard must exist');
const guard = webSource.slice(guardStart, webSource.indexOf('\n}\n', guardStart));

for (const route of [
  '/clearlogs', '/runfile', '/rawinput', '/runagentcmd', '/showimage',
  '/marauder', '/capture', '/mic', '/system', '/delete', '/set', '/settings',
  '/channelhop', '/moveFile', '/uploadFile',
]) {
  assert.ok(guard.includes(route), `mutation guard must cover ${route}`);
}
assert.match(guard, /url == "\/evilportal"\)\s*return request->hasParam\("action"\)/);

// The dispatcher must reject a non-POST method and a missing/invalid token.
const dispatcher = webSource.slice(webSource.indexOf('static void webRequestHandler'));
assert.match(dispatcher, /requestNeedsMutationGuard\(request\)/);
assert.match(dispatcher, /request->method\(\) != HTTP_POST/);
assert.match(dispatcher, /post_required/);
assert.match(dispatcher, /requestHasValidCsrf\(request\)/);
assert.match(dispatcher, /csrf_required/);

// Token issuance and the read route.
assert.match(webSource, /String WebInternal::csrfToken/);
assert.match(webSource, /esp_random\(\)/);
assert.match(webSource, /url == "\/csrf-token"/);
assert.match(webSource, /url == "\/evilportal"/);

// The upload callback runs before the dispatcher, so it enforces the token too.
assert.match(webSource, /handleUpload[\s\S]*requestHasValidCsrf\(request\)/);

// The UI must request the token and attach it.
const toast = fs.readFileSync('ui/src/core/toast-actions.js', 'utf8');
assert.match(toast, /\/csrf-token/);
assert.match(toast, /window\.redlilyPost/);
assert.match(toast, /X-CSRF-Token/);

const openapi = fs.readFileSync('openapi.yaml', 'utf8');
assert.match(openapi, /\/csrf-token:/);
assert.match(openapi, /X-CSRF-Token/);

console.log('CSRF_CONTRACT: pass');
