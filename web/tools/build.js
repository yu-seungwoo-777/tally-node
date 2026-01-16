#!/usr/bin/env node
/**
 * 빌드 스크립트
 * src/ → dist/ 복사 + pages/*.html 병합 + CSS 빌드 + JS 번들링
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import * as esbuild from 'esbuild';
import postcss from 'postcss';
import tailwindcss from 'tailwindcss';
import autoprefixer from 'autoprefixer';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const SRC_DIR = path.resolve(__dirname, '../src');
const DIST_DIR = path.resolve(__dirname, '../dist');
const PAGES_DIR = path.join(SRC_DIR, 'pages');
const EMBED_DIR = path.join(DIST_DIR, 'embed');

// 파일 복사 함수
function copyFile(src, dest) {
    const destDir = path.dirname(dest);
    if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
    }
    fs.copyFileSync(src, dest);
}

// 재귀적 복사 (pages, js, css 폴더 제외)
function copyRecursive(srcDir, destDir) {
    if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
    }

    const entries = fs.readdirSync(srcDir, { withFileTypes: true });

    for (const entry of entries) {
        if (entry.name === 'pages') continue;
        if (entry.name === 'js') continue;
        if (entry.name === 'css') continue;
        if (entry.name.endsWith('.bak')) continue;

        const srcPath = path.join(srcDir, entry.name);
        const destPath = path.join(destDir, entry.name);

        if (entry.isDirectory()) {
            copyRecursive(srcPath, destPath);
        } else {
            copyFile(srcPath, destPath);
        }
    }
}

// HTML 병합
function mergePagesToIndex() {
    const indexPath = path.join(DIST_DIR, 'index.html');
    let indexHtml = fs.readFileSync(indexPath, 'utf-8');

    const pages = ['dashboard', 'network', 'switcher', 'broadcast', 'devices', 'system', 'license'];
    let pagesHtml = '';

    for (const page of pages) {
        const pagePath = path.join(PAGES_DIR, `${page}.html`);
        if (fs.existsSync(pagePath)) {
            const pageContent = fs.readFileSync(pagePath, 'utf-8');
            pagesHtml += pageContent + '\n';
        }
    }

    indexHtml = indexHtml.replace(
        '<!-- PAGES_PLACEHOLDER: 빌드 시 pages/*.html 내용으로 교체 -->',
        pagesHtml.trim()
    );

    fs.writeFileSync(indexPath, indexHtml);
    console.log(`  Merged ${pages.length} pages`);
}

// CSS 빌드 (PostCSS + Tailwind + DaisyUI)
async function buildCss() {
    const inputCss = path.join(SRC_DIR, 'css/input.css');
    const customCss = path.join(SRC_DIR, 'css/styles.css');
    const outputCss = path.join(DIST_DIR, 'css/styles.css');

    // dist/css 폴더 생성
    fs.mkdirSync(path.dirname(outputCss), { recursive: true });

    // Tailwind CSS 빌드
    const css = fs.readFileSync(inputCss, 'utf-8');
    const result = await postcss([
        tailwindcss,
        autoprefixer
    ]).process(css, { from: undefined });

    // Tailwind 결과 + 커스텀 CSS 병합
    let finalCss = result.css;

    // 커스텀 CSS 추가
    if (fs.existsSync(customCss)) {
        finalCss += '\n' + fs.readFileSync(customCss, 'utf-8');
    }

    fs.writeFileSync(outputCss, finalCss);

    const size = fs.statSync(outputCss).size;
    console.log(`  Built css/styles.css (${(size / 1024).toFixed(1)} KB)`);
}

// JS 번들링
async function bundleJs() {
    const entryPoint = path.join(SRC_DIR, 'js/app.js');
    const outFile = path.join(DIST_DIR, 'js/app.bundle.js');

    await esbuild.build({
        entryPoints: [entryPoint],
        bundle: true,
        outfile: outFile,
        format: 'iife',
        target: 'es2020',
        minify: false,
        banner: {
            js: '// Tally Node Web Application'
        }
    });

    const bundleSize = fs.statSync(outFile).size;
    console.log(`  Bundled js/app.bundle.js (${(bundleSize / 1024).toFixed(1)} KB)`);
}

// vendor 복사
function copyVendor() {
    const vendorDir = path.join(SRC_DIR, 'vendor');
    const destVendorDir = path.join(DIST_DIR, 'vendor');

    if (!fs.existsSync(vendorDir)) return;

    fs.mkdirSync(destVendorDir, { recursive: true });
    const files = fs.readdirSync(vendorDir);
    for (const file of files) {
        copyFile(path.join(vendorDir, file), path.join(destVendorDir, file));
    }
}

// 빌드 실행
console.log('\n📦 Building...');

// dist 정리
if (fs.existsSync(DIST_DIR)) {
    fs.rmSync(DIST_DIR, { recursive: true });
}
fs.mkdirSync(EMBED_DIR, { recursive: true });

// 파일 복사
copyRecursive(SRC_DIR, DIST_DIR);
copyVendor();

// CSS 빌드
console.log('\n🎨 CSS...');
await buildCss();

// 페이지 병합
console.log('\n📄 Pages...');
mergePagesToIndex();

// JS 번들링
console.log('\n📦 JS...');
await bundleJs();

// index.html script 태그 변경
const indexPath = path.join(DIST_DIR, 'index.html');
let indexHtml = fs.readFileSync(indexPath, 'utf-8');
indexHtml = indexHtml.replace(
    '<script type="module" src="js/app.js"></script>',
    '<script src="js/app.bundle.js?v=' + Date.now() + '"></script>'
);
fs.writeFileSync(indexPath, indexHtml);

console.log('\n✅ Done!\n');
