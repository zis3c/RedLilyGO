import { expect, test } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';

// Accessibility smoke test for the dashboard shell. It runs axe-core against the
// static markup and fails on serious or critical WCAG A/AA violations. Dynamic
// data comes from the device, so this checks structure, roles, and labels.
test.beforeEach(async ({ page }) => {
  await page.route('https://**/*', (route) => route.abort());
  // The firmware serves assets from /assets/. The static test server keeps them
  // under /ui/content/assets/, so rewrite the request path to load the real CSS.
  await page.route('**/assets/**', (route) => {
    const url = new URL(route.request().url());
    return route.continue({ url: `${url.origin}/ui/content${url.pathname}` });
  });
});

test('dashboard has no serious accessibility violations', async ({ page }) => {
  await page.goto('/ui/content/index.html', { waitUntil: 'domcontentloaded' });
  const results = await new AxeBuilder({ page })
    .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa'])
    .analyze();
  const serious = results.violations.filter((v) => v.impact === 'serious' || v.impact === 'critical');
  const summary = serious.map((v) => ({
    id: v.id,
    nodes: v.nodes.map((n) => ({ target: n.target.join(' '), why: n.failureSummary })),
  }));
  expect(serious, `accessibility violations: ${JSON.stringify(summary)}`).toEqual([]);
});
