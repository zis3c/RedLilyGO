import { readFileSync } from 'node:fs';
import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  // Insert the recorder page without its scripts; this is a test fixture, not a
  // sanitizer, so the tag-filter and sanitization queries do not apply.
  // codeql[js/bad-tag-filter, js/incomplete-multi-character-sanitization]
  await page.setContent(readFileSync('../tools/macro-recorder/static/index.html', 'utf8').replace(/<script\b[\s\S]*?<\/script\s*>/gi, ''));
  await page.evaluate(() => {
    localStorage.setItem('macro_token', 'test-token');
    window.setInterval = () => 0;
    window.calls = [];
    window.fetch = async (path, options = {}) => {
      window.calls.push({ path, method: options.method || 'GET' });
      return { ok: true, json: async () => path.startsWith('/api/events')
        ? { events: [], next: 0 }
        : path === '/api/status'
          ? { recording: false, total_recorded: 0, log_path: 'test.jsonl' }
          : { ok: true } };
    };
  });
  await page.addScriptTag({ path: '../tools/macro-recorder/static/app.js' });
  await expect(page.locator('#connectionStatus')).toHaveText('Connected');
});

test('HTTP authorization failure cannot report connected or enable recording', async ({ page }) => {
  await page.evaluate(async () => {
    window.fetch = async () => ({ ok: false, status: 401, json: async () => ({ error: 'unauthorized' }) });
    await pollStatus();
    await pollEvents();
  });
  await expect(page.locator('#connectionStatus')).not.toHaveText('Connected');
  await expect(page.locator('#btnStart')).toBeDisabled();
});

for (const failure of ['http', 'semantic']) {
  test(`failed ${failure} clear keeps feed and failure message through polling`, async ({ page }) => {
    await page.evaluate(failure => {
      document.getElementById('feed').textContent = 'Retain this event';
      const original = window.fetch;
      window.fetch = (path, options) => path === '/api/clear'
        ? Promise.resolve({ ok: failure !== 'http', status: 503, json: async () => ({ ok: false, reason: 'clear_failed' }) })
        : original(path, options);
    }, failure);
    await page.locator('#btnClear').click();
    await page.evaluate(() => pollStatus());
    await expect(page.locator('#feed')).toHaveText('Retain this event');
    await expect(page.locator('#connectionStatus')).not.toHaveText('Connected');
  });
}

test('empty snapshots advance cursor and recorder restart recovers it', async ({ page }) => {
  const cursors = await page.evaluate(async () => {
    let next = 12;
    const paths = [];
    window.fetch = async path => {
      paths.push(path);
      return { ok: true, json: async () => ({ events: [], next }) };
    };
    await pollEvents();
    await pollEvents();
    next = 2;
    await pollEvents();
    await pollEvents();
    return paths;
  });
  expect(cursors).toEqual(['/api/events?since=0', '/api/events?since=12', '/api/events?since=12', '/api/events?since=0']);
});

test('late event response cannot repopulate a successfully cleared feed', async ({ page }) => {
  await page.evaluate(() => {
    const original = window.fetch;
    window.fetch = (path, options) => path.startsWith('/api/events')
      ? new Promise(resolve => { window.finishEvents = resolve; }) : original(path, options);
    window.pendingEvents = pollEvents();
  });
  await page.locator('#btnClear').click();
  await page.evaluate(async () => {
    window.finishEvents({ ok: true, json: async () => ({ events: [{ seq: 1, key: 'x', kind: 'char', time: '2026-10-01T00:00:00Z' }], next: 1 }) });
    await window.pendingEvents;
  });
  await expect(page.locator('#feed')).toBeEmpty();
});

test('slow event request cannot overlap the next poll', async ({ page }) => {
  const requests = await page.evaluate(async () => {
    let calls = 0;
    window.fetch = () => { calls++; return new Promise(resolve => { window.finishEvents = resolve; }); };
    const first = pollEvents();
    await pollEvents();
    window.finishEvents({ ok: true, json: async () => ({ events: [], next: 0 }) });
    await first;
    return calls;
  });
  expect(requests).toBe(1);
});
