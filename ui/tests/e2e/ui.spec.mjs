import { expect, test } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.route('https://**/*', (route) => route.abort());
});

test('dashboard loads with the RedLilyGO identity', async ({ page }) => {
  const response = await page.request.get('/ui/content/index.html');
  expect(response.ok()).toBeTruthy();
  const html = await response.text();
  expect(html).toContain('<title>RedLilyGO</title>');
  expect(html).toContain('id="tab-1"');
  expect(html).toContain('RedLily');
});

test('remote controls are present and mobile-sized', async ({ page }) => {
  await page.goto('/ui/vnc/index.html', { waitUntil: 'commit' });
  await expect(page).toHaveTitle('RedLilyGO Remote');
  for (const id of ['reconnectButton', 'keyboardButton', 'leftClickButton', 'rightClickButton', 'middleClickButton', 'scrollUpButton', 'scrollDownButton']) {
    await expect(page.locator(`#${id}`)).toBeVisible();
  }
  await expect(page.locator('#input_only_surface')).toBeHidden();
  const hint = page.locator('#hint');
  await expect(hint).toBeHidden();
  await page.evaluate(() => window.showRemoteHint('Success!', 100, 'success'));
  await expect(hint).toHaveText('Success!');
  await expect(hint).toHaveAttribute('data-tone', 'success');
  await expect(hint).toBeVisible();
  await expect(hint).toBeHidden({ timeout: 2000 });
  await expect(page.locator('body')).not.toContainText('VNC');
});

test('input-only remote exposes trackpad and HID controls', async ({ page }) => {
  await page.goto('/ui/vnc/index.html#input_only=true', { waitUntil: 'commit' });
  await expect(page).toHaveTitle('RedLilyGO Input Only');
  await expect(page.locator('body')).toHaveClass(/input-only/);
  await expect(page.locator('.trackpad-indicator')).toBeVisible();
  for (const id of ['lockButton', 'copyButton', 'pasteButton']) {
    await expect(page.locator(`#${id}`)).toBeVisible();
  }
  const lock = page.locator('#lockButton');
  await lock.click();
  await expect(lock).toHaveClass(/is-locked/);
  await lock.click();
  await expect(lock).not.toHaveClass(/is-locked/);
  await expect(page.locator('body')).not.toContainText('VNC');
});

test('remote reconnect control is interactive', async ({ page }) => {
  await page.goto('/ui/vnc/index.html', { waitUntil: 'commit' });
  await page.waitForTimeout(750);
  const reconnect = page.locator('#reconnectButton');
  await expect(reconnect).toBeVisible();
  await reconnect.click();
  await expect(page.locator('#status')).toContainText(/Connecting|Reconnecting/);
});

for (const mode of ['', '#input_only=true']) {
  test(`remote chrome icons hide and restore controls ${mode || 'desktop'}`, async ({ page }) => {
    await page.goto(`/ui/vnc/index.html${mode}`, { waitUntil: 'commit' });
    await page.waitForFunction(() => typeof document.getElementById('hideControlsButton').onclick === 'function');
    const hide = page.getByRole('button', { name: 'Hide controls', exact: true });
    const show = page.getByRole('button', { name: 'Show controls', exact: true });
    await expect(hide.locator('svg')).toBeVisible();
    await expect(hide).toHaveText('');
    await expect(show).toBeHidden();
    const bounds = await hide.boundingBox();
    expect(bounds.width).toBeGreaterThanOrEqual(40);
    expect(bounds.height).toBeGreaterThanOrEqual(40);
    await hide.focus();
    await hide.press('Space');
    await expect(page.locator('#top_bar')).toBeHidden();
    await expect(page.locator('#remote_controls')).toBeHidden();
    await expect(show.locator('svg')).toBeVisible();
    await expect(show).toBeFocused();
    await expect(show).toHaveAttribute('aria-expanded', 'false');
    expect(await show.boundingBox()).toEqual(bounds);
    await show.press('Enter');
    await expect(page.locator('#top_bar')).toBeVisible();
    await expect(page.locator('#remote_controls')).toBeVisible();
    await expect(show).toBeHidden();
    await expect(hide).toBeFocused();
    await expect(hide).toHaveAttribute('aria-expanded', 'true');
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await hide.click();
    await expect(page.locator('#top_bar')).toBeHidden();
    await expect(page.locator('#remote_controls')).toBeHidden();
    await expect(page.locator('#top_bar')).toHaveCSS('transition-delay', '0s');
    await show.click();
    await expect(page.locator('#top_bar')).toBeVisible();
  });
}
