#!/usr/bin/env node
/**
 * 빌드 스크립트
 * src/ → dist/ 복사
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const SRC_DIR = path.resolve(__dirname, '../src');
const DIST_DIR = path.resolve(__dirname, '../dist');
const EMBED_DIR = path.join(DIST_DIR, 'embed');

// 파일 복사 함수
function copyFile(src, dest) {
    const destDir = path.dirname(dest);
    if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
    }
    fs.copyFileSync(src, dest);
    console.log(`  ${path.relative(SRC_DIR, src)} → ${path.relative(DIST_DIR, dest)}`);
}

// 재귀적 복사
function copyRecursive(srcDir, destDir) {
    if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
    }

    const entries = fs.readdirSync(srcDir, { withFileTypes: true });

    for (const entry of entries) {
        const srcPath = path.join(srcDir, entry.name);
        const destPath = path.join(destDir, entry.name);

        if (entry.isDirectory()) {
            copyRecursive(srcPath, destPath);
        } else {
            copyFile(srcPath, destPath);
        }
    }
}

// 빌드 실행
console.log('\n📦 Building web files...');
console.log(`  Source: ${SRC_DIR}`);
console.log(`  Output: ${DIST_DIR}\n`);

// dist 폴더 정리
if (fs.existsSync(DIST_DIR)) {
    fs.rmSync(DIST_DIR, { recursive: true });
}

// dist/embed 폴더 생성
fs.mkdirSync(EMBED_DIR, { recursive: true });

// 파일 복사
copyRecursive(SRC_DIR, DIST_DIR);

console.log('\n✅ Build complete!\n');
