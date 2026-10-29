import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.setContent('<textarea id="logBox">Keep logs</textarea><span id="savedLogStatus"></span>');
  await page.evaluate(() => {
    document.documentElement.setAttribute('data-active-tab', 'tab-6');
    window.pending = [];
    window.showToast = () => {};
    window.fetch = path => new Promise(resolve => window.pending.push({ path, resolve }));
    window.redlilyCsrf = () => Promise.resolve('test-csrf-token');
    window.redlilyPost = url => window.fetch(url, { method: 'POST', headers: { 'X-CSRF-Token': 'test-csrf-token' } });
  });
  await page.addScriptTag({ path: 'src/features/logs/saved.js' });
  await page.addScriptTag({ path: 'src/features/logs/runtime-controls.js' });
});

test('latest SD log request wins over an older download', async ({ page }) => {
  await page.evaluate(() => { loadSavedLog(); loadSavedLog(); });
  await page.evaluate(() => pending[1].resolve({ ok: true, text: async () => 'Latest SD' }));
  await expect(page.locator('#logBox')).toHaveValue('Latest SD');
  await page.evaluate(() => pending[0].resolve({ ok: true, text: async () => 'Old SD' }));
  await expect(page.locator('#logBox')).toHaveValue('Latest SD');
});

test('runtime request finishing after SD load preserves SD view', async ({ page }) => {
  await page.evaluate(() => { refreshRuntimeLogs(); loadSavedLog(); });
  await page.evaluate(() => pending[1].resolve({ ok: true, text: async () => 'SD view' }));
  await expect(page.locator('#logBox')).toHaveValue('SD view');
  await page.evaluate(() => pending[0].resolve({ ok: true, json: async () => ({ logMessages: ['old runtime'] }) }));
  await expect(page.locator('#logBox')).toHaveValue('SD view');
});

test('clear invalidates an older SD download', async ({ page }) => {
  await page.evaluate(() => { loadSavedLog(); clearRuntimeLogs(); });
  await page.evaluate(() => pending[1].resolve({ ok: true, json: async () => ({ ok: true }) }));
  await expect(page.locator('#logBox')).toHaveValue('');
  await page.evaluate(() => pending[0].resolve({ ok: true, text: async () => 'Old SD' }));
  await expect(page.locator('#logBox')).toHaveValue('');
});

test('failed clear keeps displayed logs', async ({ page }) => {
  await page.evaluate(() => {
    clearRuntimeLogs();
    pending[0].resolve({ ok: false, json: async () => ({ error: 'unavailable' }) });
  });
  await expect(page.locator('#logBox')).toHaveValue('Keep logs');
});

test('slow runtime poll cannot overlap and malformed data preserves logs', async ({ page }) => {
  await page.evaluate(async () => {
    const first = refreshRuntimeLogs();
    refreshRuntimeLogs();
    window.runtimeRequests = pending.length;
    for (const request of pending) request.resolve({ ok: true, json: async () => ({ error: 'unavailable' }) });
    await first;
  });
  expect(await page.evaluate(() => window.runtimeRequests)).toBe(1);
  await expect(page.locator('#logBox')).toHaveValue('Keep logs');
  await page.evaluate(() => { refreshRuntimeLogs(); });
  expect(await page.evaluate(() => pending.length)).toBe(2);
});
