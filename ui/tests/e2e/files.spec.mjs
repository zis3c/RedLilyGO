import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.setContent(`
    <div id="tree"></div>
    <input id="fileSelectionBox">
    <select id="fileUploadDirectory"><option value="Auto">Auto folder</option></select>
    <button id="fileRunButton" disabled>Run</button>
    <button id="fileEditButton" disabled>Preview</button>
    <button id="filePortalButton" disabled>Portal</button>
    <button id="fileDownloadButton" disabled>Download</button>
    <button id="fileDeleteButton" disabled>Delete</button>
    <pre id="filePreviewContent" data-path="/Scripts/run.ds"></pre>
    <textarea id="filePreviewEditor"></textarea>
    <div id="filePreviewEditorShell" class="hidden"></div>
    <button id="filePreviewSaveButton"></button>
    <div id="filePreviewModal" class="is-open" style="display:flex"></div>
    <div id="filePreviewModeLabel"></div>
    <div id="filePreviewTitle"></div>
    <div id="filePreviewActions"></div>
    <button id="filePreviewMenuButton"></button>`);
  await page.evaluate(() => {
    window.requests = [];
    window.actions = [];
    window.showToast = () => {};
    window.fireQuickAction = async url => { window.actions.push(url); return true; };
    window.redlilyCsrf = () => Promise.resolve('test-csrf-token');
    window.redlilyPost = async (url, options = {}) => window.fetch(url, { ...options, method: 'POST' });
    window.fetch = async (url, options = {}) => {
      window.requests.push({ url, method: options.method || 'GET' });
      if (url === '/evilportal/files') return { ok: true, json: async () => [] };
      if (url.startsWith('/files?directory=')) {
        const directory = decodeURIComponent(url.split('=')[1]);
        if (directory === '/') return { ok: true, json: async () => ({
          storage: { available: true },
          directories: ['/__proto__', '/constructor', '/Scripts'],
          fileDetails: [
            { name: 'run.ds', path: '/Scripts/run.ds', size: 12 },
            { name: 'raw.pcap', path: '/Scripts/raw.pcap', size: 128 },
          ],
        }) };
        return { ok: true, json: async () => ({
          storage: { available: true }, directories: [],
          fileDetails: [{ name: 'one.ds', path: directory + '/one.ds', size: 4 }],
        }) };
      }
      return { ok: true, json: async () => ({ ok: true }) };
    };
  });
  await page.addScriptTag({ path: 'src/features/tools/payloads.js' });
  await page.addScriptTag({ path: 'src/features/files/editor-upload.js' });
  await page.evaluate(() => {
    document.dispatchEvent(new Event('DOMContentLoaded'));
    window.refreshIdeFiles();
  });
  await expect(page.locator('#tree [data-path]')).toHaveCount(2);
});

test('file tree safely displays reserved-looking directory names', async ({ page }) => {
  await expect(page.locator('#tree [data-folder-path="/__proto__"]')).toHaveCount(1);
  await expect(page.locator('#tree [data-folder-path="/constructor"]')).toHaveCount(1);
  await page.locator('#tree [data-folder-path="/__proto__"]').click();
  await expect(page.locator('#tree [data-path="/__proto__/one.ds"]')).toHaveCount(1);
});

test('a DuckyScript selection enables Run and sends its full path', async ({ page }) => {
  await page.locator('#tree [data-path="/Scripts/run.ds"]').click();
  await expect(page.locator('#fileRunButton')).toBeEnabled();
  await page.locator('#fileRunButton').click();
  await expect.poll(() => page.evaluate(() => window.actions)).toEqual([
    '/runfile?filename=%2FScripts%2Frun.ds',
  ]);
});

test('binary capture can download but not preview', async ({ page }) => {
  await page.locator('#tree [data-path="/Scripts/raw.pcap"]').click();
  await expect(page.locator('#fileDownloadButton')).toBeEnabled();
  await expect(page.locator('#fileEditButton')).toBeDisabled();
  await expect(page.locator('#fileRunButton')).toBeDisabled();
});

test('folder names remain literal text in upload options', async ({ page }) => {
  const folder = '/<option value="spoof">Injected</option>';
  await page.evaluate(folder => {
    window.fetch = async url => url.startsWith('/files?')
      ? { ok: true, json: async () => ({ storage: { available: true }, directories: [folder], fileDetails: [] }) }
      : { ok: true, json: async () => [] };
    window.refreshIdeFiles();
  }, folder);
  await expect(page.locator('#fileUploadDirectory option')).toHaveCount(2);
  await expect(page.locator('#fileUploadDirectory option').nth(1)).toHaveText(folder);
  expect(await page.locator('#fileUploadDirectory option').nth(1).getAttribute('value')).toBe(folder);
});

test('older file refresh cannot replace a newer listing or retain stale selection', async ({ page }) => {
  await page.locator('#tree [data-path="/Scripts/run.ds"]').click();
  await page.evaluate(() => {
    window.listings = [];
    window.fetch = url => url.startsWith('/files?')
      ? new Promise(resolve => window.listings.push(resolve))
      : Promise.resolve({ ok: true, json: async () => [] });
    window.refreshIdeFiles();
    window.refreshIdeFiles();
  });
  await expect(page.locator('#fileRunButton')).toBeDisabled();
  await page.evaluate(() => window.listings[1]({ ok: true, json: async () => ({
    storage: { available: true }, directories: ['/New'],
    fileDetails: [{ name: 'new.ds', path: '/New/new.ds' }],
  }) }));
  await expect(page.locator('#tree [data-path="/New/new.ds"]')).toHaveCount(1);
  await page.evaluate(() => window.listings[0]({ ok: true, json: async () => ({
    storage: { available: true }, directories: ['/Old'],
    fileDetails: [{ name: 'old.ds', path: '/Old/old.ds' }],
  }) }));
  // Drain promise continuations before checking the stale completion.
  await page.evaluate(() => new Promise(resolve => setTimeout(resolve, 0)));
  await expect(page.locator('#tree [data-path="/New/new.ds"]')).toHaveCount(1);
  await expect(page.locator('#fileUploadDirectory option[value="/Old"]')).toHaveCount(0);
});

test('stale folder download cannot change upload folders after refresh', async ({ page }) => {
  await page.evaluate(() => {
    window.fetch = url => url.includes('%2F__proto__')
      ? new Promise(resolve => { window.oldFolder = resolve; })
      : Promise.resolve({ ok: true, json: async () => url === '/evilportal/files' ? [] : ({
        storage: { available: true }, directories: ['/New'], fileDetails: [],
      }) });
  });
  await page.locator('#tree [data-folder-path="/__proto__"]').click();
  await page.evaluate(() => window.refreshIdeFiles());
  await expect(page.locator('#tree [data-folder-path="/New"]')).toHaveCount(1);
  await page.evaluate(() => window.oldFolder({ ok: true, json: async () => ({
    storage: { available: true }, directories: ['/Stale'], fileDetails: [],
  }) }));
  await page.evaluate(() => new Promise(resolve => setTimeout(resolve, 0)));
  await expect(page.locator('#fileUploadDirectory option[value="/Stale"]')).toHaveCount(0);
});

test('saving an edited file preserves its selected path', async ({ page }) => {
  await page.locator('#fileSelectionBox').fill('/Scripts/run.ds');
  await page.evaluate(() => setPreviewContent(document.getElementById('filePreviewContent'), 'original'));
  await page.locator('#filePreviewEditor').fill('REM saved regression fixture');
  await page.evaluate(() => window.savePreviewFile());
  await expect.poll(() => page.evaluate(() => window.requests.filter(x => x.method === 'POST'))).toEqual([
    { url: '/uploadFile?replace=1&path=%2FScripts%2Frun.ds', method: 'POST' },
  ]);
  await expect(page.locator('#filePreviewContent')).toContainText('REM saved regression fixture');
});

test('save targets the loaded file even after tree selection changes', async ({ page }) => {
  await page.evaluate(() => {
    setPreviewContent(document.getElementById('filePreviewContent'), 'original');
    document.getElementById('fileSelectionBox').value = '/Scripts/other.ds';
  });
  await page.locator('#filePreviewEditor').fill('edited loaded file');
  await page.evaluate(() => window.savePreviewFile());
  expect(await page.evaluate(() => window.requests.filter(x => x.method === 'POST'))).toEqual([
    { url: '/uploadFile?replace=1&path=%2FScripts%2Frun.ds', method: 'POST' },
  ]);
});

test('older downloads cannot overwrite a newer preview, including CRLF content', async ({ page }) => {
  await page.evaluate(() => {
    window.downloads = [];
    window.fetch = () => new Promise(resolve => window.downloads.push(resolve));
    document.getElementById('fileSelectionBox').value = '/Scripts/old.ds';
    window.oldPreview = window.previewSelectedFile();
    document.getElementById('fileSelectionBox').value = '/Scripts/new.ds';
    window.newPreview = window.previewSelectedFile();
  });
  await page.evaluate(async () => {
    window.downloads[1]({ ok: true, text: async () => 'new\r\nfile' });
    await window.newPreview;
    window.downloads[0]({ ok: true, text: async () => 'old file' });
    await window.oldPreview;
  });
  await expect(page.locator('#filePreviewEditor')).toHaveValue('new\nfile');
  expect(await page.evaluate(() => previewHasUnsavedChanges())).toBe(false);
  await expect(page.locator('#filePreviewTitle')).toHaveText('Preview: /Scripts/new.ds');
});

test('failed download cannot be edited or saved', async ({ page }) => {
  await page.evaluate(async () => {
    document.getElementById('fileSelectionBox').value = '/Scripts/run.ds';
    window.fetch = async () => ({ ok: false, status: 404 });
    await window.previewSelectedFile();
    window.editPreviewFile();
    window.saveError = await window.savePreviewFile().catch(error => error.message);
  });
  await expect(page.locator('#filePreviewContent')).toContainText('HTTP 404');
  expect(await page.evaluate(() => window.saveError)).toBe('No file loaded');
  await expect(page.locator('#filePreviewEditorShell')).toHaveClass(/hidden/);
});

test('pending save is deduplicated and cannot replace another open file', async ({ page }) => {
  await page.evaluate(() => {
    var content = document.getElementById('filePreviewContent');
    setPreviewContent(content, 'original');
    document.getElementById('filePreviewEditor').value = 'saved old file';
    window.saveRequests = 0;
    var releaseCsrf, releaseFetch;
    var csrfGate = new Promise(resolve => { releaseCsrf = resolve; });
    var fetchGate = new Promise(resolve => { releaseFetch = resolve; });
    window.redlilyCsrf = () => csrfGate.then(() => 'test-token');
    var realFetch = window.fetch.bind(window);
    window.fetch = (url, options) => {
      var method = (options && options.method) || 'GET';
      if (method === 'POST' && /\/uploadFile/.test(url)) {
        window.saveRequests++;
        return fetchGate.then(() => ({ ok: true, json: () => Promise.resolve({ ok: true }) }));
      }
      if (method === 'GET' && /\/download\?filename=/.test(url)) {
        return Promise.resolve({ ok: true, text: () => Promise.resolve('new file contents') });
      }
      return realFetch(url, options);
    };
    window.firstSave = window.savePreviewFile();
    window.secondSave = window.savePreviewFile();
    window.closeFilePreview(true);
    document.getElementById('fileSelectionBox').value = '/Scripts/new.ds';
    window.chunkedSave = { releaseCsrf, releaseFetch };
  });
  await page.evaluate(async () => {
    const preview = window.previewSelectedFile(true);
    await new Promise(resolve => setTimeout(resolve, 0));
    window.chunkedSave.releaseCsrf();
    await new Promise(resolve => setTimeout(resolve, 0));
    window.chunkedSave.releaseFetch();
    await Promise.all([preview, window.firstSave, window.secondSave]);
  });
  expect(await page.evaluate(() => window.saveRequests)).toBe(1);
  await expect(page.locator('#filePreviewEditor')).toHaveValue('new file contents');
  expect(await page.locator('#filePreviewContent').getAttribute('data-saved-text')).toBe('new file contents');
});

test('opening another file requires confirmation before losing a draft', async ({ page }) => {
  await page.evaluate(() => {
    setPreviewContent(document.getElementById('filePreviewContent'), 'original');
    document.getElementById('filePreviewEditor').value = 'keep draft';
    document.getElementById('fileSelectionBox').value = '/Scripts/other.ds';
    window.confirmations = 0;
    window.showConfirmModal = () => { window.confirmations++; };
    window.previewSelectedFile();
  });
  expect(await page.evaluate(() => window.confirmations)).toBe(1);
  await expect(page.locator('#filePreviewEditor')).toHaveValue('keep draft');
  expect(await page.evaluate(() => window.requests.filter(x => x.url.startsWith('/download')))).toEqual([]);
});

test('unsaved edits survive preview mode and still require discard confirmation', async ({ page }) => {
  await page.evaluate(() => {
    setPreviewContent(document.getElementById('filePreviewContent'), 'original');
    window.editPreviewFile();
    window.confirmations = 0;
    window.showConfirmModal = () => { window.confirmations++; };
  });
  await page.locator('#filePreviewEditor').fill('unsaved draft');
  await page.evaluate(() => window.editPreviewFile());
  await expect(page.locator('#filePreviewContent')).toContainText('unsaved draft');
  await page.evaluate(() => window.closeFilePreview());
  expect(await page.evaluate(() => window.confirmations)).toBe(1);
  await expect(page.locator('#filePreviewModal')).toHaveClass('is-open');
  await page.evaluate(() => window.editPreviewFile());
  await expect(page.locator('#filePreviewEditor')).toHaveValue('unsaved draft');
});

test('failed save preserves the draft and unsaved state', async ({ page }) => {
  await page.evaluate(() => {
    document.getElementById('fileSelectionBox').value = '/Scripts/run.ds';
    setPreviewContent(document.getElementById('filePreviewContent'), 'original');
    window.editPreviewFile();
    window.fetch = async () => ({ ok: false, status: 500 });
  });
  await page.locator('#filePreviewEditor').fill('keep this draft');
  await page.evaluate(() => window.savePreviewFile().catch(() => {}));
  await expect(page.locator('#filePreviewEditor')).toHaveValue('keep this draft');
  expect(await page.evaluate(() => previewHasUnsavedChanges())).toBe(true);
  await expect(page.locator('#filePreviewSaveButton')).toBeEnabled();
});

test('edits made during a save remain unsaved after that request completes', async ({ page }) => {
  await page.evaluate(() => {
    document.getElementById('fileSelectionBox').value = '/Scripts/run.ds';
    setPreviewContent(document.getElementById('filePreviewContent'), 'original');
    window.editPreviewFile();
    window.fetch = () => new Promise(resolve => { window.resolveSave = resolve; });
  });
  await page.locator('#filePreviewEditor').fill('submitted draft');
  await page.evaluate(() => { window.pendingSave = window.savePreviewFile(); });
  await page.locator('#filePreviewEditor').fill('newer draft');
  await page.evaluate(async () => {
    window.resolveSave({ ok: true, json: async () => ({ ok: true }) });
    await window.pendingSave;
  });
  await expect(page.locator('#filePreviewEditor')).toHaveValue('newer draft');
  expect(await page.evaluate(() => previewHasUnsavedChanges())).toBe(true);
  expect(await page.locator('#filePreviewContent').getAttribute('data-saved-text')).toBe('submitted draft');
  await expect(page.locator('#filePreviewEditorShell')).not.toHaveClass(/hidden/);
});
