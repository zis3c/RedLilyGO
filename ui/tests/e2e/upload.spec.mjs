import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.setContent(`
    <input type="file" id="fileInputBox"><span id="fileInputText">Selected</span>
    <select id="fileUploadDirectory"><option>Auto</option></select>
    <div id="fileUploadProgressBar" class="opacity-0"><div></div></div>`);
  await page.evaluate(() => {
    window.uploads = [];
    window.toasts = [];
    window.showToast = text => window.toasts.push(text);
    window.refreshIdeFiles = () => {};
    window.redlilyCsrf = () => Promise.resolve('test-csrf-token');
    window.XMLHttpRequest = class extends EventTarget {
      constructor() { super(); this.upload = new EventTarget(); }
      open(method, path) { this.method = method; this.path = path; }
      setRequestHeader(name, value) { this.csrfHeader = { name, value }; }
      send(body) { this.body = body; window.uploads.push(this); }
      finish(type, status = 0) {
        this.status = status;
        this.dispatchEvent(new Event(type));
        this.dispatchEvent(new Event('loadend'));
      }
    };
  });
  await page.addScriptTag({ path: 'src/features/files/editor-upload.js' });
  await page.locator('#fileInputBox').setInputFiles({ name: 'first.ds', mimeType: 'text/plain', buffer: Buffer.from('REM first') });
});

test('duplicate upload reuses the active request', async ({ page }) => {
  const requests = await page.evaluate(async () => {
    uploadFile(); uploadFile();
    await Promise.resolve();
    return uploads.length;
  });
  expect(requests).toBe(1);
});

test('completed upload preserves a newer file selection', async ({ page }) => {
  await page.evaluate(() => uploadFile());
  await page.waitForFunction(() => window.uploads.length >= 1);
  await page.locator('#fileInputBox').setInputFiles({ name: 'second.ds', mimeType: 'text/plain', buffer: Buffer.from('REM second') });
  await page.evaluate(() => uploads[0].finish('load', 200));
  expect(await page.locator('#fileInputBox').evaluate(input => input.files[0]?.name)).toBe('second.ds');
});

test('old progress hide timer is cancelled when a new upload starts', async ({ page }) => {
  await page.evaluate(() => {
    window.cleared = [];
    window.setTimeout = () => 123;
    window.clearTimeout = id => window.cleared.push(id);
    uploadFile();
  });
  await page.waitForFunction(() => window.uploads.length >= 1);
  await page.evaluate(() => uploads[0].finish('load', 200));
  await page.locator('#fileInputBox').setInputFiles({ name: 'second.ds', mimeType: 'text/plain', buffer: Buffer.from('REM second') });
  await page.evaluate(() => uploadFile());
  await page.waitForFunction(() => window.uploads.length >= 2);
  expect(await page.evaluate(() => window.cleared)).toContain(123);
  await expect(page.locator('#fileUploadProgressBar')).not.toHaveClass(/opacity-0/);
});

test('timeout keeps selection, reports failure and permits retry', async ({ page }) => {
  await page.evaluate(() => uploadFile());
  await page.waitForFunction(() => window.uploads.length >= 1);
  await page.evaluate(() => uploads[0].finish('timeout'));
  expect(await page.evaluate(() => uploads[0].timeout)).toBe(60000);
  await expect(page.locator('#fileUploadProgressBar')).toHaveClass(/opacity-0/);
  expect(await page.locator('#fileInputBox').evaluate(input => input.files[0]?.name)).toBe('first.ds');
  expect(await page.evaluate(() => window.toasts)).toContain('Upload timed out. Try again.');
  await page.evaluate(() => uploadFile());
  await page.waitForFunction(() => window.uploads.length >= 2);
  expect(await page.evaluate(() => uploads.length)).toBe(2);
});
