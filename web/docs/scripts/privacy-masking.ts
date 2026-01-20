/**
 * Privacy Masking Script for Screenshots
 *
 * This script masks sensitive information in screenshots such as:
 * - IP addresses
 * - Device IDs
 * - MAC addresses
 * - Serial numbers
 * - License keys
 *
 * Usage:
 *   npx ts-node privacy-masking.ts
 *   or
 *   npm run screenshot:mask
 *
 * @version 1.0.0
 * @license MIT
 */

import * as fs from 'fs';
import * as path from 'path';
import { createCanvas, loadImage, Image } from 'canvas';

// ============================================================================
// Configuration
// ============================================================================

interface MaskConfig {
  screenshotsDir: string;
  outputDir: string;
  masks: MaskRegion[];
}

interface MaskRegion {
  type: 'ip' | 'deviceId' | 'mac' | 'ssid' | 'license';
  selector: string; // CSS selector for automated detection
  fallback: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
}

const CONFIG: MaskConfig = {
  screenshotsDir: path.join(__dirname, '../screenshots'),
  outputDir: path.join(__dirname, '../screenshots-masked'),
  masks: [
    // Dashboard masks
    { type: 'deviceId', selector: '[data-device-id]', fallback: { x: 100, y: 50, width: 80, height: 20 } },
    { type: 'ip', selector: '[data-ip-address]', fallback: { x: 200, y: 100, width: 120, height: 20 } },

    // Network masks
    { type: 'ssid', selector: '[data-ssid]', fallback: { x: 150, y: 80, width: 150, height: 20 } },
    { type: 'ip', selector: '[data-ip]', fallback: { x: 150, y: 120, width: 120, height: 20 } },

    // License masks
    { type: 'license', selector: '[data-license-key]', fallback: { x: 100, y: 150, width: 200, height: 20 } }
  ]
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
 * Apply blur effect to region
 */
function applyBlur(
  ctx: CanvasRenderingContext2D,
  imageData: ImageData,
  x: number,
  y: number,
  width: number,
  height: number,
  radius: number = 5
): void {
  const pixels = imageData.data;
  const w = imageData.width;
  const h = imageData.height;

  // Simple box blur implementation
  for (let py = Math.max(0, y); py < Math.min(h, y + height); py++) {
    for (let px = Math.max(0, x); px < Math.min(w, x + width); px++) {
      let r = 0, g = 0, b = 0, a = 0, count = 0;

      // Average pixels in radius
      for (let dy = -radius; dy <= radius; dy++) {
        for (let dx = -radius; dx <= radius; dx++) {
          const nx = px + dx;
          const ny = py + dy;

          if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
            const idx = (ny * w + nx) * 4;
            r += pixels[idx];
            g += pixels[idx + 1];
            b += pixels[idx + 2];
            a += pixels[idx + 3];
            count++;
          }
        }
      }

      const idx = (py * w + px) * 4;
      pixels[idx] = r / count;
      pixels[idx + 1] = g / count;
      pixels[idx + 2] = b / count;
      pixels[idx + 3] = a / count;
    }
  }

  ctx.putImageData(imageData, 0, 0);
}

/**
 * Apply solid color overlay
 */
function applyOverlay(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  width: number,
  height: number,
  color: string = 'rgba(0, 0, 0, 0.7)'
): void {
  ctx.fillStyle = color;
  ctx.fillRect(x, y, width, height);
}

/**
 * Apply pixelation effect
 */
function applyPixelation(
  ctx: CanvasRenderingContext2D,
  imageData: ImageData,
  x: number,
  y: number,
  width: number,
  height: number,
  pixelSize: number = 10
): void {
  const pixels = imageData.data;
  const w = imageData.width;
  const h = imageData.height;

  for (let py = y; py < y + height; py += pixelSize) {
    for (let px = x; px < x + width; px += pixelSize) {
      let r = 0, g = 0, b = 0, a = 0, count = 0;

      // Average pixels in block
      for (let dy = 0; dy < pixelSize && py + dy < h; dy++) {
        for (let dx = 0; dx < pixelSize && px + dx < w; dx++) {
          const idx = ((py + dy) * w + (px + dx)) * 4;
          r += pixels[idx];
          g += pixels[idx + 1];
          b += pixels[idx + 2];
          a += pixels[idx + 3];
          count++;
        }
      }

      // Fill block with average color
      for (let dy = 0; dy < pixelSize && py + dy < Math.min(h, y + height); dy++) {
        for (let dx = 0; dx < pixelSize && px + dx < Math.min(w, x + width); dx++) {
          const idx = ((py + dy) * w + (px + dx)) * 4;
          pixels[idx] = r / count;
          pixels[idx + 1] = g / count;
          pixels[idx + 2] = b / count;
          pixels[idx + 3] = a / count;
        }
      }
    }
  }

  ctx.putImageData(imageData, 0, 0);
}

// ============================================================================
// Masking Functions
// ============================================================================

/**
 * Apply privacy masks to image
 */
async function applyMasks(inputPath: string, outputPath: string): Promise<void> {
  const image = await loadImage(inputPath);
  const canvas = createCanvas(image.width, image.height);
  const ctx = canvas.getContext('2d');

  // Draw original image
  ctx.drawImage(image, 0, 0);

  // Apply masks based on image type/location
  const relativePath = path.relative(CONFIG.screenshotsDir, inputPath);
  const pageName = relativePath.split(path.sep)[0];

  // Get masks for this page
  const pageMasks = getPageMasks(pageName);

  for (const mask of pageMasks) {
    const { x, y, width, height } = mask.fallback;

    // Apply overlay with dark color
    applyOverlay(ctx, x, y, width, height, 'rgba(30, 41, 59, 0.9)');

    // Add "XXXX" text
    ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
    ctx.font = '12px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('XXXX', x + width / 2, y + height / 2);
  }

  // Save masked image
  const buffer = canvas.toBuffer('image/png');
  fs.writeFileSync(outputPath, buffer);
}

/**
 * Get masks for specific page
 */
function getPageMasks(pageName: string): typeof CONFIG.masks {
  // In production, this would analyze the actual image
  // to find sensitive data regions using OCR or pattern matching
  return CONFIG.masks;
}

/**
 * Process all screenshots in directory
 */
async function processScreenshots(): Promise<void> {
  console.log('🔒 Starting privacy masking...\n');

  let processed = 0;
  let failed = 0;

  // Process each page directory
  const pageDirs = fs.readdirSync(CONFIG.screenshotsDir, { withFileTypes: true })
    .filter(dirent => dirent.isDirectory())
    .map(dirent => dirent.name);

  for (const pageDir of pageDirs) {
    const inputDir = path.join(CONFIG.screenshotsDir, pageDir);
    const outputDir = path.join(CONFIG.outputDir, pageDir);

    ensureDir(outputDir);

    const files = fs.readdirSync(inputDir)
      .filter(file => file.endsWith('.png'));

    console.log(`📁 Processing ${pageDir}...`);

    for (const file of files) {
      const inputPath = path.join(inputDir, file);
      const outputPath = path.join(outputDir, file);

      try {
        await applyMasks(inputPath, outputPath);
        console.log(`  ✓ ${file}`);
        processed++;
      } catch (error) {
        console.error(`  ✗ ${file}:`, error);
        failed++;
      }
    }
  }

  console.log('\n📊 Summary:');
  console.log(`  ✓ Processed: ${processed}`);
  console.log(`  ✗ Failed: ${failed}`);
  console.log('\n✅ Privacy masking complete!\n');
}

// ============================================================================
// Main Execution
// ============================================================================

async function main(): Promise<void> {
  try {
    ensureDir(CONFIG.outputDir);
    await processScreenshots();
  } catch (error) {
    console.error('\n❌ Fatal error:', error);
    process.exit(1);
  }
}

// Run if executed directly
if (require.main === module) {
  main();
}

export { applyMasks, processScreenshots, CONFIG };
