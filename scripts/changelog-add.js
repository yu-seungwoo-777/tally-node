#!/usr/bin/env node

/**
 * Changelog Management CLI
 *
 * 대화형으로 changelog.json을 생성/수정합니다.
 *
 * Usage: npm run changelog-add
 *
 * Features:
 * - 버전별 변경 로그 추가
 * - TX/RX 별도 관리
 * - 한/영双语 지원
 */

const readline = require('readline');
const { readFileSync, writeFileSync, existsSync } = require('fs');
const { join } = require('path');

const CHANGELOG_PATH = join(__dirname, '..', 'changelog.json');

/**
 * Create readline interface
 */
function createInterface() {
  return readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });
}

/**
 * Prompt user for input
 */
function prompt(rl, question, defaultValue = '') {
  return new Promise((resolve) => {
    const promptText = defaultValue
      ? `${question} [${defaultValue}]: `
      : `${question}: `;

    rl.question(promptText, (answer) => {
      resolve(answer || defaultValue);
    });
  });
}

/**
 * Prompt for yes/no confirmation
 */
function confirm(rl, question) {
  return new Promise((resolve) => {
    rl.question(`${question} (y/n): `, (answer) => {
      resolve(answer.toLowerCase() === 'y' || answer.toLowerCase() === 'yes');
    });
  });
}

/**
 * Load existing changelog or create new structure
 */
function loadChangelog() {
  if (existsSync(CHANGELOG_PATH)) {
    const data = readFileSync(CHANGELOG_PATH, 'utf-8');
    return JSON.parse(data);
  }

  return {
    versions: []
  };
}

/**
 * Save changelog to file
 */
function saveChangelog(changelog) {
  // Sort versions by date (newest first)
  changelog.versions.sort((a, b) => {
    return new Date(b.date) - new Date(a.date);
  });

  writeFileSync(CHANGELOG_PATH, JSON.stringify(changelog, null, 2), 'utf-8');
}

/**
 * Check if version already exists
 */
function versionExists(changelog, version) {
  return changelog.versions.some(v => v.version === version);
}

/**
 * Get changes input from user
 */
async function getChangesInput(rl, label) {
  console.log(`\n${label} 변경사항을 입력하세요.`);

  const changes = [];
  let i = 1;

  while (true) {
    const change = await prompt(rl, `  ${i}. (빈 줄 입력으로 완료)`);

    if (!change.trim()) {
      break;
    }

    changes.push(change.trim());
    i++;
  }

  return changes;
}

/**
 * Get fixes input from user
 */
async function getFixesInput(rl, label) {
  console.log(`\n${label} 버그 수정 사항을 입력하세요.`);

  const fixes = [];
  let i = 1;

  while (true) {
    const fix = await prompt(rl, `  ${i}. (빈 줄 입력으로 완료)`);

    if (!fix.trim()) {
      break;
    }

    fixes.push(fix.trim());
    i++;
  }

  return fixes.length > 0 ? fixes : null;
}

/**
 * Get English translation input
 */
async function getEnglishInput(rl, koreanTitle, koreanChanges, koreanFixes) {
  console.log('\n--- 영어 번역 ---');
  console.log('번역을 입력하세요. 빈 줄 입력 시 건너뜁니다.\n');

  const title = await prompt(rl, 'Title (English)', koreanTitle);
  const hasChanges = await confirm(rl, '변경사항 번역 입력?');

  let changes = [];
  if (hasChanges) {
    let i = 1;
    while (true) {
      const change = await prompt(rl, `  ${i}. (빈 줄 입력으로 완료)`);
      if (!change.trim()) break;
      changes.push(change.trim());
      i++;
    }
  }

  const hasFixes = koreanFixes && await confirm(rl, '버그 수정 번역 입력?');

  let fixes = null;
  if (hasFixes) {
    fixes = [];
    let i = 1;
    while (true) {
      const fix = await prompt(rl, `  ${i}. (빈 줄 입력으로 완료)`);
      if (!fix.trim()) break;
      fixes.push(fix.trim());
      i++;
    }
  }

  return {
    title,
    changes: changes.length > 0 ? changes : null,
    fixes
  };
}

/**
 * Get board input (TX/RX)
 */
async function getBoardInput(rl, boardName) {
  console.log(`\n${'='.repeat(60)}`);
  console.log(`${boardName} 보드 변경사항`);
  console.log(`${'='.repeat(60)}`);

  // 한국어 입력
  const koTitle = await prompt(rl, 'Title (한국어)');
  const koChanges = await getChangesInput(rl, '변경사항');
  const koFixes = await getFixesInput(rl, '버그 수정');

  // 영어 입력
  const en = await getEnglishInput(rl, koTitle, koChanges, koFixes);

  return {
    ko: {
      title: koTitle,
      changes: koChanges,
      fixes: koFixes
    },
    en: {
      title: en.title,
      changes: en.changes || koChanges, // Fallback to Korean
      fixes: en.fixes || koFixes
    }
  };
}

/**
 * Display existing versions
 */
function displayVersions(changelog) {
  if (changelog.versions.length === 0) {
    console.log('등록된 버전이 없습니다.');
    return;
  }

  console.log('\n등록된 버전:');
  console.log('-'.repeat(60));

  changelog.versions.forEach((v, i) => {
    const txTitle = v.tx?.ko?.title || '-';
    const rxTitle = v.rx?.ko?.title || '-';
    console.log(`  ${i + 1}. ${v.version} (${v.date})`);
    console.log(`     TX: ${txTitle}`);
    console.log(`     RX: ${rxTitle}`);
  });
  console.log('');
}

/**
 * Main function
 */
async function main() {
  const rl = createInterface();

  console.log('='.repeat(60));
  console.log('Changelog Management CLI');
  console.log('='.repeat(60));

  const changelog = loadChangelog();
  displayVersions(changelog);

  // Get version
  const today = new Date().toISOString().split('T')[0];
  const defaultVersion = changelog.versions.length > 0
    ? changelog.versions[0].version
    : '1.0.0';

  const version = await prompt(rl, '\n버전 (예: 1.0.0)', defaultVersion);
  const date = await prompt(rl, '날짜 (YYYY-MM-DD)', today);

  // Check if version exists
  if (versionExists(changelog, version)) {
    console.log(`\n⚠️  버전 ${version}이 이미 존재합니다.`);
    const overwrite = await confirm(rl, '덮어쓰시겠습니까?');

    if (!overwrite) {
      console.log('취소되었습니다.');
      rl.close();
      return;
    }

    // Remove existing version
    changelog.versions = changelog.versions.filter(v => v.version !== version);
  }

  // TX input
  const hasTx = await confirm(rl, '\nTX 보드 변경사항이 있습니까?');
  let txData = null;

  if (hasTx) {
    txData = await getBoardInput(rl, 'TX');
  }

  // RX input
  const hasRx = await confirm(rl, '\nRX 보드 변경사항이 있습니까?');
  let rxData = null;

  if (hasRx) {
    rxData = await getBoardInput(rl, 'RX');
  }

  if (!txData && !rxData) {
    console.log('\n⚠️  변경사항이 없습니다. 취소합니다.');
    rl.close();
    return;
  }

  // Create version entry
  const versionEntry = {
    version,
    date
  };

  if (txData) {
    versionEntry.tx = txData;
  }

  if (rxData) {
    versionEntry.rx = rxData;
  }

  // Add to changelog
  changelog.versions.push(versionEntry);

  // Save
  saveChangelog(changelog);

  console.log('\n' + '='.repeat(60));
  console.log('✅ Changelog가 저장되었습니다!');
  console.log(`   파일: ${CHANGELOG_PATH}`);
  console.log('='.repeat(60));

  // Display preview
  console.log('\n미리보기:');
  console.log(JSON.stringify(versionEntry, null, 2));

  rl.close();
}

// Run
main().catch(error => {
  console.error('Error:', error);
  process.exit(1);
});
