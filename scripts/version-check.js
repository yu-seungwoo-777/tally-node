#!/usr/bin/env node

/**
 * Version Check CLI
 *
 * 프로젝트 내 모든 버전 파일의 일관성을 확인합니다.
 *
 * Usage: npm run version
 */

const { readFileSync, existsSync } = require('fs');
const { join } = require('path');

const PROJECT_ROOT = join(__dirname, '..');

/**
 * Get version from a file with regex pattern
 */
function getVersionFromFile(filePath, pattern, description) {
  if (!existsSync(filePath)) {
    return { version: null, source: description, status: 'not_found' };
  }

  try {
    const content = readFileSync(filePath, 'utf-8');
    const match = content.match(pattern);
    if (match && match[1]) {
      return { version: match[1], source: description, status: 'found' };
    }
    return { version: null, source: description, status: 'not_found' };
  } catch (e) {
    return { version: null, source: description, status: 'error', error: e.message };
  }
}

/**
 * Get version from web bundle
 */
function getVersionFromWebBundle() {
  const bundlePath = join(PROJECT_ROOT, 'web', 'dist', 'js', 'app.bundle.js');

  if (!existsSync(bundlePath)) {
    return { version: null, source: 'web/dist/js/app.bundle.js', status: 'not_found' };
  }

  try {
    const content = readFileSync(bundlePath, 'utf-8');
    const firmwareMatch = content.match(/version:"(\d+\.\d+\.\d+)",(?:loraChipType|deviceId|config:)/);
    if (firmwareMatch && firmwareMatch[1]) {
      return { version: firmwareMatch[1], source: 'web/dist/js/app.bundle.js', status: 'found' };
    }
    return { version: null, source: 'web/dist/js/app.bundle.js', status: 'not_found' };
  } catch (e) {
    return { version: null, source: 'web/dist/js/app.bundle.js', status: 'error', error: e.message };
  }
}

/**
 * Get version from PIO binary
 */
function getVersionFromPIOBinary(buildEnv) {
  const { execSync } = require('child_process');
  const binaryPath = join(PROJECT_ROOT, '.pio', 'build', buildEnv, 'firmware.bin');

  if (!existsSync(binaryPath)) {
    return { version: null, source: `${buildEnv}/firmware.bin`, status: 'not_found' };
  }

  try {
    const output = execSync(`strings "${binaryPath}" 2>/dev/null | grep -E "^[0-9]+\\.[0-9]+\\.[0-9]+$" | grep -vE "^5\\." | head -1 || echo ""`, {
      encoding: 'utf-8',
      stdio: ['pipe', 'pipe', 'pipe']
    });

    const version = output.trim();
    if (version && /^\d+\.\d+\.\d+$/.test(version)) {
      return { version, source: `${buildEnv}/firmware.bin`, status: 'found' };
    }
    return { version: null, source: `${buildEnv}/firmware.bin`, status: 'not_found' };
  } catch (e) {
    return { version: null, source: `${buildEnv}/firmware.bin`, status: 'error', error: e.message };
  }
}

/**
 * Main check function
 */
function main() {
  console.log('='.repeat(60));
  console.log('           펌웨어 버전 일관성 체크                    ');
  console.log('='.repeat(60));

  const versionChecks = [];

  // 1. 소스 파일들에서 버전 확인
  const sourceChecks = [
    getVersionFromFile(
      join(PROJECT_ROOT, 'platformio.ini'),
      /-DFIRMWARE_VERSION=\\"([0-9.]+)\\"/,
      'platformio.ini'
    ),
    getVersionFromFile(
      join(PROJECT_ROOT, 'components', '00_common', 'app_types', 'include', 'app_types.h'),
      /#define FIRMWARE_VERSION "([0-9.]+)"/,
      'app_types.h'
    ),
    getVersionFromFile(
      join(PROJECT_ROOT, 'web', 'package.json'),
      /"version":\s*"([0-9.]+)"/,
      'web/package.json'
    ),
    getVersionFromFile(
      join(PROJECT_ROOT, 'web', 'src', 'js', 'modules', 'state.js'),
      /version:\s*'([0-9.]+)'/,
      'web/src/js/modules/state.js'
    ),
    getVersionFromFile(
      join(PROJECT_ROOT, 'changelog.json'),
      /"version":\s*"([0-9.]+)"/,
      'changelog.json'
    ),
    getVersionFromWebBundle()
  ];

  console.log('\n📄 소스 파일:');
  console.log('-'.repeat(60));
  sourceChecks.forEach(check => {
    versionChecks.push(check);
    const status = check.status === 'found' ? '✓' : check.status === 'not_found' ? '⚠' : '❌';
    const version = check.version || '없음';
    console.log(`  ${status} ${check.source}: ${version}`);
  });

  // 2. PIO 빌드된 바이너리에서 버전 확인
  console.log('\n📦 PIO 빌드 바이너리:');
  console.log('-'.repeat(60));
  const txBinary = getVersionFromPIOBinary('eora_s3_tx');
  const rxBinary = getVersionFromPIOBinary('eora_s3_rx');

  versionChecks.push(txBinary, rxBinary);

  const txStatus = txBinary.status === 'found' ? '✓' : '⚠';
  const rxStatus = rxBinary.status === 'found' ? '✓' : '⚠';
  const txVersion = txBinary.version || '빌드 안됨';
  const rxVersion = rxBinary.version || '빌드 안됨';

  console.log(`  ${txStatus} TX (${txBinary.source}): ${txVersion}`);
  console.log(`  ${rxStatus} RX (${rxBinary.source}): ${rxVersion}`);

  // 3. 버전 일치 여부 확인
  console.log('\n' + '='.repeat(60));
  const foundVersions = versionChecks
    .filter(c => c.status === 'found' && c.version)
    .map(c => c.version);

  const uniqueVersions = [...new Set(foundVersions)];
  const allMatch = uniqueVersions.length <= 1;

  if (allMatch && foundVersions.length > 0) {
    console.log(`  ✅ 모든 버전이 일치합니다: ${uniqueVersions[0]}`);
    console.log('='.repeat(60));
    process.exit(0);
  } else if (foundVersions.length === 0) {
    console.log('  ⚠️  버전을 찾을 수 없습니다.');
    console.log('='.repeat(60));
    process.exit(1);
  }

  // 버전 불일치
  console.log('  ❌ 버전 불일치가 발견되었습니다!');
  console.log('='.repeat(60));
  console.log('\n발견된 버전:');
  uniqueVersions.forEach(v => {
    const sources = versionChecks.filter(c => c.version === v).map(c => c.source);
    console.log(`  ${v}:`);
    sources.forEach(s => console.log(`    - ${s}`));
  });
  console.log('\n모든 버전이 일치하도록 설정해주세요.');
  console.log('='.repeat(60));
  process.exit(1);
}

// Run
if (require.main === module) {
  main();
}

// Export for use in other scripts
module.exports = {
  getVersionFromFile,
  getVersionFromWebBundle,
  getVersionFromPIOBinary,
  checkAllVersions: function() {
    const versionChecks = [];

    const sourceChecks = [
      getVersionFromFile(
        join(PROJECT_ROOT, 'platformio.ini'),
        /-DFIRMWARE_VERSION=\\"([0-9.]+)\\"/,
        'platformio.ini'
      ),
      getVersionFromFile(
        join(PROJECT_ROOT, 'components', '00_common', 'app_types', 'include', 'app_types.h'),
        /#define FIRMWARE_VERSION "([0-9.]+)"/,
        'app_types.h'
      ),
      getVersionFromFile(
        join(PROJECT_ROOT, 'web', 'package.json'),
        /"version":\s*"([0-9.]+)"/,
        'web/package.json'
      ),
      getVersionFromFile(
        join(PROJECT_ROOT, 'web', 'src', 'js', 'modules', 'state.js'),
        /version:\s*'([0-9.]+)'/,
        'web/src/js/modules/state.js'
      ),
      getVersionFromFile(
        join(PROJECT_ROOT, 'changelog.json'),
        /"version":\s*"([0-9.]+)"/,
        'changelog.json'
      ),
      getVersionFromWebBundle()
    ];

    sourceChecks.forEach(check => versionChecks.push(check));

    const txBinary = getVersionFromPIOBinary('eora_s3_tx');
    const rxBinary = getVersionFromPIOBinary('eora_s3_rx');
    versionChecks.push(txBinary, rxBinary);

    const foundVersions = versionChecks
      .filter(c => c.status === 'found' && c.version)
      .map(c => c.version);

    const uniqueVersions = [...new Set(foundVersions)];

    // changelog만 다른지 확인
    const changelogCheck = versionChecks.find(c => c.source === 'changelog.json');
    const otherChecks = versionChecks.filter(c => c.source !== 'changelog.json');
    const otherVersions = [...new Set(otherChecks.filter(c => c.version).map(c => c.version))];

    // changelog만 버전이 다르고, 나머지는 모두 일치하는 경우
    if (uniqueVersions.length === 2 && changelogCheck && changelogCheck.version && otherVersions.length === 1) {
      // changelog만 다름 - main 버전 반환
      return {
        version: otherVersions[0],
        changelogVersion: changelogCheck.version,
        changelogOnlyMismatch: true
      };
    }

    if (otherVersions.length === 1) {
      return {
        version: otherVersions[0],
        changelogOnlyMismatch: false
      };
    }

    const errorMsg = '버전 불일치가 발견되었습니다:\n' +
      uniqueVersions.map(v => {
        const sources = versionChecks.filter(c => c.version === v).map(c => c.source);
        return `  ${v}: ${sources.join(', ')}`;
      }).join('\n');
    throw new Error(errorMsg);
  }
};
