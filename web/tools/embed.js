#!/usr/bin/env node
/**
 * 임베드 스크립트
 * dist/ 파일 → C 헤더로 변환 → dist/embed/
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const DIST_DIR = path.resolve(__dirname, '../dist');
const EMBED_DIR = path.join(DIST_DIR, 'embed');

// 파일을 C 헤더로 변환
function embedFile(filePath, outputDir) {
    const content = fs.readFileSync(filePath);
    const basename = path.basename(filePath);
    const varName = basename.replace(/[.-]/g, '_');

    // 바이트 배열을 C 형식으로 변환
    const hexArray = [];
    for (let i = 0; i < content.length; i++) {
        hexArray.push('0x' + content[i].toString(16).padStart(2, '0'));
    }

    // C 배열 생성 (12바이트씩 줄바꿈)
    const lines = [];
    for (let i = 0; i < hexArray.length; i += 12) {
        lines.push(hexArray.slice(i, i + 12).join(', '));
    }
    const formattedArray = lines.join(',\n    ');

    const header = `// Auto-generated from ${basename}
// DO NOT EDIT

#pragma once

#include <stddef.h>

static const unsigned char ${varName}_data[] = {
    ${formattedArray}
};

static const size_t ${varName}_len = ${content.length};
`;

    const outputPath = path.join(outputDir, `${path.parse(basename).name}_h.h`);
    fs.writeFileSync(outputPath, header);
    console.log(`  ${basename} → ${path.basename(outputPath)}`);
}

// 재귀적 파일 탐색
function findFiles(dir, extensions) {
    const files = [];

    function traverse(currentDir) {
        const entries = fs.readdirSync(currentDir, { withFileTypes: true });

        for (const entry of entries) {
            const fullPath = path.join(currentDir, entry.name);

            if (entry.isDirectory()) {
                // embed 폴더는 건너뜀
                if (entry.name !== 'embed') {
                    traverse(fullPath);
                }
            } else {
                const ext = path.extname(entry.name);
                if (extensions.includes(ext)) {
                    files.push(fullPath);
                }
            }
        }
    }

    traverse(dir);
    return files;
}

// 임베드 실행
console.log('\n🔨 Embedding files for ESP32...');
console.log(`  Source: ${DIST_DIR}`);
console.log(`  Output: ${EMBED_DIR}\n`);

// embed 폴더 정리
if (fs.existsSync(EMBED_DIR)) {
    fs.readdirSync(EMBED_DIR).forEach(file => {
        fs.unlinkSync(path.join(EMBED_DIR, file));
    });
} else {
    fs.mkdirSync(EMBED_DIR, { recursive: true });
}

// 파일 임베드
const files = findFiles(DIST_DIR, ['.html', '.css', '.js', '.svg', '.ico']);

if (files.length === 0) {
    console.log('  ⚠️  No files found. Run "npm run build" first.\n');
    process.exit(1);
}

for (const file of files) {
    embedFile(file, EMBED_DIR);
}

// 인덱스 파일 생성
const indexHeader = `// Auto-generated file list
// DO NOT EDIT

#pragma once

`;
const includes = files.map(f => {
    const basename = path.basename(f);
    const name = path.parse(basename).name;
    return `#include "${name}_h.h"`;
}).join('\n');

fs.writeFileSync(path.join(EMBED_DIR, 'static_files.h'), indexHeader + includes);

console.log(`\n✅ Embedded ${files.length} files!\n`);
