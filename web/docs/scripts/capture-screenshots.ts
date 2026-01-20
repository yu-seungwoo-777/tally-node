/**
 * Playwright Screenshot Capture Script for Tally Node Web UI
 *
 * This script automates the capture of screenshots for UI documentation.
 * It supports multiple viewports, devices, and page states.
 *
 * Usage:
 *   npx ts-node capture-screenshots.ts
 *   or
 *   npm run screenshot:capture
 *
 * @version 1.0.0
 * @license MIT
 */

import { chromium, Browser, Page, BrowserContext } from 'playwright';
import * as fs from 'fs';
import * as path from 'path';

// ============================================================================
// Configuration
// ============================================================================

interface ScreenshotConfig {
  baseUrl: string;
  screenshotsDir: string;
  viewports: ViewportConfig[];
  pages: PageConfig[];
  delay: number;
}

interface ViewportConfig {
  name: string;
  width: number;
  height: number;
  deviceScaleFactor: number;
  isMobile: boolean;
}

interface PageConfig {
  name: string;
  url: string;
  views: ViewConfig[];
}

interface ViewConfig {
  name: string;
  selector?: string;
  waitForSelector?: string;
  actions?: PageAction[];
}

interface PageAction {
  type: 'click' | 'hover' | 'fill' | 'select';
  selector: string;
  value?: string;
}

const CONFIG: ScreenshotConfig = {
  baseUrl: 'http://localhost:8080',
  screenshotsDir: path.join(__dirname, '../screenshots'),
  viewports: [
    {
      name: 'desktop',
      width: 1920,
      height: 1080,
      deviceScaleFactor: 2,
      isMobile: false
    },
    {
      name: 'mobile',
      width: 375,
      height: 667,
      deviceScaleFactor: 2,
      isMobile: true
    }
  ],
  pages: [
    {
      name: 'dashboard',
      url: '/#dashboard',
      views: [
        { name: 'main' }
      ]
    },
    {
      name: 'network',
      url: '/#network',
      views: [
        { name: 'main' },
        { name: 'settings' }
      ]
    },
    {
      name: 'switcher',
      url: '/#switcher',
      views: [
        { name: 'main' },
        { name: 'dual-mode' }
      ]
    },
    {
      name: 'broadcast',
      url: '/#broadcast',
      views: [
        { name: 'main' },
        { name: 'scan' }
      ]
    },
    {
      name: 'devices',
      url: '/#devices',
      views: [
        { name: 'main' },
        { name: 'device-card' }
      ]
    },
    {
      name: 'license',
      url: '/#license',
      views: [
        { name: 'main' }
      ]
    },
    {
      name: 'system',
      url: '/#system',
      views: [
        { name: 'main' }
      ]
    }
  ],
  delay: 1000
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Ensure directory exists, create if not
 */
function ensureDir(dir: string): void {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

/**
 * Generate screenshot filename
 */
function generateFilename(
  page: string,
  view: string,
  device: string,
  state: string = 'default',
  index: number = 1
): string {
  const paddedIndex = index.toString().padStart(2, '0');
  return `${page}-${view}-${device}-${state}-${paddedIndex}.png`;
}

/**
 * Wait for page to be stable
 */
async function waitForPageStable(page: Page): Promise<void> {
  await page.waitForLoadState('networkidle');
  await page.waitForTimeout(CONFIG.delay);
}

/**
 * Mask sensitive data in screenshot
 * This is a placeholder - actual masking should be done in post-processing
 */
async function maskSensitiveData(
  page: Page,
  screenshotPath: string
): Promise<void> {
  // Placeholder for privacy masking
  // In production, this would use image processing libraries
  // to blur or cover sensitive information
}

// ============================================================================
// Screenshot Capture Functions
// ============================================================================

/**
 * Capture screenshot for a specific view
 */
async function captureView(
  page: Page,
  pageConfig: PageConfig,
  viewConfig: ViewConfig,
  viewport: ViewportConfig
): Promise<void> {
  const filename = generateFilename(
    pageConfig.name,
    viewConfig.name,
    viewport.name
  );
  const outputPath = path.join(
    CONFIG.screenshotsDir,
    pageConfig.name,
    filename
  );

  ensureDir(path.dirname(outputPath));

  try {
    // Navigate to page
    await page.goto(CONFIG.baseUrl + pageConfig.url, {
      waitUntil: 'networkidle'
    });

    // Wait for stability
    await waitForPageStable(page);

    // Execute actions if any
    if (viewConfig.actions) {
      for (const action of viewConfig.actions) {
        switch (action.type) {
          case 'click':
            await page.click(action.selector);
            break;
          case 'hover':
            await page.hover(action.selector);
            break;
          case 'fill':
            if (action.value) {
              await page.fill(action.selector, action.value);
            }
            break;
          case 'select':
            if (action.value) {
              await page.selectOption(action.selector, action.value);
            }
            break;
        }
      }
      await waitForPageStable(page);
    }

    // Capture full page screenshot
    await page.screenshot({
      path: outputPath,
      fullPage: true,
      animations: 'disabled'
    });

    console.log(`✓ Captured: ${filename}`);
  } catch (error) {
    console.error(`✗ Failed to capture ${filename}:`, error);
  }
}

/**
 * Capture all views for a page
 */
async function capturePage(
  browser: Browser,
  pageConfig: PageConfig,
  viewport: ViewportConfig
): Promise<void> {
  const context = await browser.newContext({
    viewport: {
      width: viewport.width,
      height: viewport.height
    },
    deviceScaleFactor: viewport.deviceScaleFactor,
    isMobile: viewport.isMobile
  });

  const page = await context.newPage();

  for (const viewConfig of pageConfig.views) {
    await captureView(page, pageConfig, viewConfig, viewport);
  }

  await context.close();
}

/**
 * Capture all screenshots
 */
async function captureAllScreenshots(): Promise<void> {
  console.log('🎬 Starting screenshot capture...\n');

  const browser = await chromium.launch({
    headless: true
  });

  let totalCaptured = 0;
  let totalFailed = 0;

  for (const viewport of CONFIG.viewports) {
    console.log(`\n📱 Capturing for ${viewport.name}...`);

    for (const pageConfig of CONFIG.pages) {
      try {
        await capturePage(browser, pageConfig, viewport);
        totalCaptured += pageConfig.views.length;
      } catch (error) {
        console.error(`✗ Failed to capture ${pageConfig.name}:`, error);
        totalFailed += pageConfig.views.length;
      }
    }
  }

  await browser.close();

  console.log('\n\n📊 Summary:');
  console.log(`  ✓ Captured: ${totalCaptured}`);
  console.log(`  ✗ Failed: ${totalFailed}`);
  console.log('\n✅ Screenshot capture complete!\n');
}

// ============================================================================
// Main Execution
// ============================================================================

async function main(): Promise<void> {
  try {
    // Check if dev server is running
    console.log('🔍 Checking dev server...');
    const testBrowser = await chromium.launch();
    const testContext = await testBrowser.newContext();
    const testPage = await testContext.newPage();

    try {
      await testPage.goto(CONFIG.baseUrl, { timeout: 5000 });
      console.log('✓ Dev server is running\n');
    } catch (error) {
      console.error('✗ Dev server is not running!');
      console.error(`\nPlease start the dev server first:`);
      console.error(`  cd web && npm run dev\n`);
      process.exit(1);
    } finally {
      await testContext.close();
      await testBrowser.close();
    }

    // Capture all screenshots
    await captureAllScreenshots();
  } catch (error) {
    console.error('\n❌ Fatal error:', error);
    process.exit(1);
  }
}

// Run if executed directly
if (require.main === module) {
  main();
}

export { captureAllScreenshots, CONFIG };
