import fs from 'node:fs';
import assert from 'node:assert/strict';

const workflow = fs.readFileSync('.github/workflows/release.yml', 'utf8');
const launcher = fs.readFileSync('tools/start-agent.ps1', 'utf8');
const checker = fs.readFileSync('tools/release-check.ps1', 'utf8');

assert.match(workflow, /release-check\.ps1/);
assert.match(workflow, /SHA256SUMS\.txt/);
assert.match(workflow, /THIRD_PARTY_NOTICES\.md/);
assert.match(workflow, /third-party-licenses\.zip/);
assert.ok(workflow.indexOf('Generate artifact checksums') < workflow.indexOf('softprops/action-gh-release'), 'checksums must be generated before release upload');
assert.match(workflow, /find artifacts -type f/);
assert.match(workflow, /No firmware or Agent artifacts found/);
assert.match(workflow, /artifact_files\+=\("artifacts\/release-manifest\.json"\)/,
  'published manifest must be included in SHA256SUMS');
assert.match(checker, /Agent AssemblyVersion/);
assert.match(checker, /Agent protocol version metadata/);
assert.match(launcher, /RedLilyGO\.Agent\.exe/);
assert.match(launcher, /vid=\$Vid/);
assert.match(checker, /VERSION/);
assert.match(fs.readFileSync('tools/package-release.ps1', 'utf8'), /THIRD_PARTY_LICENSES\.zip/);
assert.match(checker, /v\$version/);

console.log('RELEASE_CONTRACT: pass');
