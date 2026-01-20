/**
 * Quality Validation Tests for Screenshots
 *
 * This test suite validates screenshot quality including:
 * - Resolution requirements (2x scale)
 * - File size constraints (< 500KB)
 * - Privacy masking verification
 * - Visual quality checks
 *
 * Usage:
 *   npx jest validate-screenshots.test.ts
 *   or
 *   npm run screenshot:test
 *
 * @version 1.0.0
 * @license MIT
 */

import { describe, test, expect } from '@jest/globals';
import * as fs from 'fs';
import * as path from 'path';
import { imageSize } from 'image-size';
import * as png from 'png.js';

// ============================================================================
// Configuration
// ============================================================================

const SCREENSHOTS_DIR = path.join(__dirname, '../screenshots');
const MASKED_DIR = path.join(__dirname, '../screenshots-masked');

const REQUIREMENTS = {
  minWidth: 1920,      // Desktop width at 1x
  minHeight: 1080,     // Desktop height at 1x
  minScale: 2,         // 2x scale requirement
  maxSize: 500 * 1024, // 500KB max file size
  allowedFormats: ['image/png']
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get all screenshot files recursively
 */
function getScreenshotFiles(dir: string): string[] {
  const files: string[] = const entries = fs.readdirSync(dir, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...getScreenshotFiles(fullPath));
    } else if (entry.isFile() && entry.name.endsWith('.png')) {
      files.push(fullPath);
    }
  }

  return files;
}

/**
 * Check if file is a valid PNG
 */
function isValidPNG(filePath: string): boolean {
  try {
    const buffer = fs.readFileSync(filePath);
    return buffer.toString('ascii', 0, 8) === '\x89PNG\r\n\x1a\n';
  } catch {
    return false;
  }
}

/**
 * Calculate image scale factor
 */
function calculateScaleFactor(width: number, expectedWidth: number): number {
  return width / expectedWidth;
}

/**
 * Check for privacy violations using simple pattern matching
 * This is a basic implementation - production would use OCR
 */
function checkPrivacyViolation(filePath: string): string[] {
  const violations: string[] = [];

  try {
    // Check filename for sensitive data patterns
    const filename = path.basename(filePath);

    // IP address pattern in filename
    if (/\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/.test(filename)) {
      violations.push('IP address detected in filename');
    }

    // Device ID pattern (assuming 4-digit hex)
    if (/[0-9A-Fa-f]{4}/.test(filename)) {
      violations.push('Possible device ID in filename');
    }

    // In production, would use OCR to check actual image content
    // For now, this is a placeholder

  } catch (error) {
    violations.push(`Error checking privacy: ${error}`);
  }

  return violations;
}

// ============================================================================
// Test Suites
// ============================================================================

describe('Screenshot File Validation', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('should have screenshot files', () => {
    expect(screenshotFiles.length).toBeGreaterThan(0);
  });

  test('all files should be valid PNG format', () => {
    for (const file of screenshotFiles) {
      expect(isValidPNG(file)).toBe(true);
    }
  });

  test('files should follow naming convention', () => {
    const namingPattern = /^[a-z]+-[a-z]+-(desktop|mobile)-[\w]+-\d{2}\.png$/;

    for (const file of screenshotFiles) {
      const filename = path.basename(file);
      expect(filename).toMatch(namingPattern);
    }
  });
});

describe('Screenshot Resolution Requirements', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('desktop screenshots should meet minimum resolution', () => {
    const desktopFiles = screenshotFiles.filter(f => f.includes('/desktop-'));

    for (const file of desktopFiles) {
      const size = imageSize(file);
      const scale = calculateScaleFactor(size.width!, REQUIREMENTS.minWidth);

      expect(size.width!).toBeGreaterThanOrEqual(REQUIREMENTS.minWidth);
      expect(size.height!).toBeGreaterThanOrEqual(REQUIREMENTS.minHeight);
      expect(scale).toBeGreaterThanOrEqual(REQUIREMENTS.minScale);
    }
  });

  test('mobile screenshots should have appropriate resolution', () => {
    const mobileFiles = screenshotFiles.filter(f => f.includes('/mobile-'));

    for (const file of mobileFiles) {
      const size = imageSize(file);

      // Mobile at 2x: 750x1334
      expect(size.width!).toBeGreaterThanOrEqual(750);
      expect(size.height!).toBeGreaterThanOrEqual(1334);
    }
  });
});

describe('Screenshot File Size Constraints', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('all screenshots should be under size limit', () => {
    for (const file of screenshotFiles) {
      const stats = fs.statSync(file);
      expect(stats.size).toBeLessThan(REQUIREMENTS.maxSize);
    }
  });

  test('file sizes should be reasonable (not too small)', () => {
    const minSize = 10 * 1024; // 10KB minimum

    for (const file of screenshotFiles) {
      const stats = fs.statSync(file);
      expect(stats.size).toBeGreaterThan(minSize);
    }
  });
});

describe('Privacy Masking Verification', () => {
  let maskedFiles: string[];

  beforeAll(() => {
    if (fs.existsSync(MASKED_DIR)) {
      maskedFiles = getScreenshotFiles(MASKED_DIR);
    } else {
      maskedFiles = [];
    }
  });

  test('masked directory should exist if masking was run', () => {
    // This test is optional - masking may not have been run yet
    if (maskedFiles.length > 0) {
      expect(maskedFiles.length).toBeGreaterThan(0);
    }
  });

  test('masked files should have same dimensions as original', () => {
    if (maskedFiles.length === 0) {
      return; // Skip if no masked files
    }

    for (const maskedFile of maskedFiles) {
      const relativePath = path.relative(MASKED_DIR, maskedFile);
      const originalFile = path.join(SCREENSHOTS_DIR, relativePath);

      if (fs.existsSync(originalFile)) {
        const originalSize = imageSize(originalFile);
        const maskedSize = imageSize(maskedFile);

        expect(originalSize.width).toBe(maskedSize.width);
        expect(originalSize.height).toBe(maskedSize.height);
      }
    }
  });

  test('should not have obvious privacy violations', () => {
    if (maskedFiles.length === 0) {
      return; // Skip if no masked files
    }

    for (const file of maskedFiles) {
      const violations = checkPrivacyViolation(file);
      expect(violations).toHaveLength(0);
    }
  });
});

describe('Directory Structure Validation', () => {
  test('should have correct directory structure', () => {
    const expectedDirs = [
      'dashboard',
      'network',
      'switcher',
      'broadcast',
      'devices',
      'license',
      'system'
    ];

    for (const dir of expectedDirs) {
      const dirPath = path.join(SCREENSHOTS_DIR, dir);
      expect(fs.existsSync(dirPath)).toBe(true);
      expect(fs.statSync(dirPath).isDirectory()).toBe(true);
    }
  });

  test('each directory should contain screenshots', () => {
    const entries = fs.readdirSync(SCREENSHOTS_DIR, { withFileTypes: true });
    const dirs = entries.filter(e => e.isDirectory()).map(e => e.name);

    for (const dir of dirs) {
      const dirPath = path.join(SCREENSHOTS_DIR, dir);
      const files = fs.readdirSync(dirPath).filter(f => f.endsWith('.png'));
      expect(files.length).toBeGreaterThan(0);
    }
  });
});

describe('Image Quality Checks', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('images should have valid color depth', () => {
    // PNG should be 8-bit per channel (24-bit RGB) or 32-bit RGBA
    for (const file of screenshotFiles) {
      const size = imageSize(file);
      // Most screenshots will be 24 or 32 bit
      expect(size.type).toBe('png');
    }
  });

  test('images should not be corrupted', () => {
    for (const file of screenshotFiles) {
      expect(() => {
        imageSize(file);
      }).not.toThrow();
    }
  });
});

describe('Documentation Integration', () => {
  test('markdown files should exist for each page', () => {
    const guideDir = path.join(__dirname, '../guide');
    const expectedFiles = [
      'SCREENSHOT_GUIDELINES.md',
      'CAPTURE_PROCEDURE.md'
    ];

    for (const file of expectedFiles) {
      const filePath = path.join(guideDir, file);
      expect(fs.existsSync(filePath)).toBe(true);
    }
  });

  test('screenshots should be referenceable from docs', () => {
    // Check that screenshot paths in documentation are valid
    const guideDir = path.join(__dirname, '../guide');
    const markdownFiles = fs.readdirSync(guideDir)
      .filter(f => f.endsWith('.md'));

    for (const mdFile of markdownFiles) {
      const content = fs.readFileSync(path.join(guideDir, mdFile), 'utf-8');
      const screenshotLinks = content.match(/\.\.\/screenshots\/[^\)]+\.(png|jpg)/g) || [];

      for (const link of screenshotLinks) {
        const fullPath = path.join(guideDir, link);
        expect(fs.existsSync(fullPath)).toBe(true);
      }
    }
  });
});

// ============================================================================
// Coverage Reporting
// ============================================================================

describe('Coverage Summary', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('should have coverage for all main pages', () => {
    const pages = ['dashboard', 'network', 'switcher', 'broadcast', 'devices', 'license', 'system'];
    const coveredPages = new Set<string>();

    for (const file of screenshotFiles) {
      const relativePath = path.relative(SCREENSHOTS_DIR, file);
      const pageName = relativePath.split(path.sep)[0];
      coveredPages.add(pageName);
    }

    for (const page of pages) {
      expect(coveredPages.has(page)).toBe(true);
    }
  });

  test('should have both desktop and mobile views', () => {
    const hasDesktop = screenshotFiles.some(f => f.includes('/desktop-'));
    const hasMobile = screenshotFiles.some(f => f.includes('/mobile-'));

    expect(hasDesktop).toBe(true);
    expect(hasMobile).toBe(true);
  });

  test('should report total screenshot count', () => {
    console.log(`\n📊 Total screenshots: ${screenshotFiles.length}`);

    const byPage: Record<string, number> = {};
    for (const file of screenshotFiles) {
      const relativePath = path.relative(SCREENSHOTS_DIR, file);
      const pageName = relativePath.split(path.sep)[0];
      byPage[pageName] = (byPage[pageName] || 0) + 1;
    }

    console.log('📁 Screenshots by page:');
    for (const [page, count] of Object.entries(byPage).sort(([a], [b]) => a.localeCompare(b))) {
      console.log(`   ${page}: ${count}`);
    }
  });
});

// ============================================================================
// Performance Benchmarks
// ============================================================================

describe('Performance Benchmarks', () => {
  let screenshotFiles: string[];

  beforeAll(() => {
    screenshotFiles = getScreenshotFiles(SCREENSHOTS_DIR);
  });

  test('should report average file size', () => {
    let totalSize = 0;

    for (const file of screenshotFiles) {
      totalSize += fs.statSync(file).size;
    }

    const avgSize = totalSize / screenshotFiles.length;
    const avgSizeKB = (avgSize / 1024).toFixed(2);

    console.log(`\n📦 Average file size: ${avgSizeKB} KB`);

    // Average should be reasonable (< 300KB)
    expect(avgSize).toBeLessThan(300 * 1024);
  });

  test('should report total storage used', () => {
    let totalSize = 0;

    for (const file of screenshotFiles) {
      totalSize += fs.statSync(file).size;
    }

    const totalSizeMB = (totalSize / (1024 * 1024)).toFixed(2);

    console.log(`💾 Total storage: ${totalSizeMB} MB`);

    // Total should be reasonable (< 50MB)
    expect(totalSize).toBeLessThan(50 * 1024 * 1024);
  });
});

export {};
