import assert from 'node:assert/strict';
import fs from 'node:fs';

const config = fs.readFileSync('platformio.ini', 'utf8');
for (const [url] of config.matchAll(/https:\/\/github\.com\/[^\s;]+/g)) {
  assert.ok(/#[a-f0-9]{40}$/.test(url) || /#\d+\.\d+\.\d+$/.test(url) || /\/archive\/refs\/tags\/[^/]+\.zip$/.test(url), `Unpinned Git dependency: ${url}`);
}
for (const line of config.split(/\r?\n/).map(line => line.trim())) {
  if (/^[\w-]+\/[\w-]+/.test(line) && !line.includes('https://')) {
    assert.match(line, /@\s*\d+\.\d+\.\d+/, `Registry dependency needs an exact version: ${line}`);
  }
}
console.log('DEPENDENCY_CONTRACT: pass');
