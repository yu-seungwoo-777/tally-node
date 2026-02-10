#!/usr/bin/env node

/**
 * Version Bump CLI v2.0
 *
 * 대화형으로 버전을 관리하고 changelog를 생성합니다.
 *
 * Usage: npm run version
 *
 * Features:
 * - 버전 일괄 업데이트 (platformio.ini, app_types.h, package.json, changelog.json)
 * - Git 커밋 로그 기반 변경사항 자동 추천
 * - Claude AI를 활용한 changelog 작성
 * - 한/영双语 지원
 */

const readline = require('readline');
const { readFileSync, writeFileSync, existsSync } = require('fs');
const { join } = require('path');
const { execSync } = require('child_process');

const PROJECT_ROOT = join(__dirname, '..');
const CHANGELOG_PATH = join(PROJECT_ROOT, 'changelog.json');
const PLATFORMIO_INI_PATH = join(PROJECT_ROOT, 'platformio.ini');
const APP_TYPES_PATH = join(PROJECT_ROOT, 'components/00_common/app_types/include/app_types.h');
const WEB_PACKAGE_PATH = join(PROJECT_ROOT, 'web/package.json');

// Anthropic API 설정
const ANTHROPIC_API_KEY = process.env.ANTHROPIC_API_KEY || '';

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
 * Get current version from files
 */
function getCurrentVersions() {
  const versions = {};

  // platformio.ini
  try {
    const pioContent = readFileSync(PLATFORMIO_INI_PATH, 'utf-8');
    const match = pioContent.match(/-DFIRMWARE_VERSION=\\"([0-9.]+)\\"/);
    if (match) {
      versions.platformio = match[1];
    }
  } catch (e) {
    // Ignore
  }

  // app_types.h
  try {
    const appTypesContent = readFileSync(APP_TYPES_PATH, 'utf-8');
    const match = appTypesContent.match(/#define FIRMWARE_VERSION "([0-9.]+)"/);
    if (match) {
      versions.appTypes = match[1];
    }
  } catch (e) {
    // Ignore
  }

  // web/package.json
  try {
    const pkgContent = readFileSync(WEB_PACKAGE_PATH, 'utf-8');
    const pkg = JSON.parse(pkgContent);
    if (pkg.version) {
      versions.web = pkg.version;
    }
  } catch (e) {
    // Ignore
  }

  // changelog.json (latest)
  try {
    const changelogContent = readFileSync(CHANGELOG_PATH, 'utf-8');
    const changelog = JSON.parse(changelogContent);
    if (changelog.versions && changelog.versions.length > 0) {
      versions.changelog = changelog.versions[0].version;
    }
  } catch (e) {
    // Ignore
  }

  return versions;
}

/**
 * Display current versions
 */
function displayCurrentVersions(versions) {
  console.log('\n현재 버전 상태:');
  console.log('-'.repeat(60));

  const files = [
    { name: 'platformio.ini', version: versions.platformio },
    { name: 'app_types.h', version: versions.appTypes },
    { name: 'web/package.json', version: versions.web },
    { name: 'changelog.json', version: versions.changelog }
  ];

  let allMatch = true;
  const firstVersion = versions.platformio || versions.appTypes || versions.web || 'unknown';

  files.forEach(({ name, version }) => {
    const status = version === firstVersion ? '✓' : '⚠';
    const display = version || '없음';
    console.log(`  ${status} ${name}: ${display}`);
    if (version !== firstVersion && version) {
      allMatch = false;
    }
  });

  console.log('-'.repeat(60));

  if (allMatch && firstVersion !== 'unknown') {
    console.log(`  ✅ 모든 파일이 버전 ${firstVersion}으로 일치합니다.\n`);
  } else {
    console.log(`  ⚠️  버전이 불일치합니다. 일괄 업데이트가 필요합니다.\n`);
  }

  return firstVersion;
}

/**
 * Get git commits since last version
 */
function getGitCommitsSince(lastVersion) {
  try {
    let commits = [];

    // 먼저 태그 존재 여부 확인
    const tagExists = lastVersion !== 'unknown' && (() => {
      try {
        execSync(`git rev-parse v${lastVersion}^{tag} --quiet`, { cwd: PROJECT_ROOT, stdio: 'ignore' });
        return true;
      } catch {
        return false;
      }
    })();

    if (tagExists) {
      // 태그가 있으면 태그 이후 커밋 조회
      try {
        const output = execSync(
          `git log v${lastVersion}..HEAD --pretty=format:"%h|%s|%an|%ad" --date=short -20`,
          { cwd: PROJECT_ROOT, encoding: 'utf-8', stdio: ['ignore', 'pipe', 'pipe'] }
        );
        commits = output.trim().split('\n').filter(line => line.trim());
      } catch (e) {
        // 태그는 있지만 범위가 비어있을 수 있음
        commits = [];
      }
    }

    // 태그가 없거나 결과가 없으면 최근 커밋 조회
    if (commits.length === 0) {
      const output = execSync(
        'git log --pretty=format:"%h|%s|%an|%ad" --date=short -20',
        { cwd: PROJECT_ROOT, encoding: 'utf-8', stdio: ['ignore', 'pipe', 'pipe'] }
      );
      commits = output.trim().split('\n').filter(line => line.trim());
    }

    return commits.map(line => {
      const [hash, subject, author, date] = line.split('|');
      return { hash, subject, author, date };
    });
  } catch (e) {
    console.log(`  ⚠️  Git 로그를 가져올 수 없습니다: ${e.message}`);
    return [];
  }
}

/**
 * Get changelog suggestions from Claude based on git commits
 */
async function getClaudeCommitSuggestions(commits) {
  if (!ANTHROPIC_API_KEY) {
    return null;
  }

  console.log('\n🤖 Claude AI가 커밋 로그를 분석 중...');

  const commitList = commits.map((c, i) =>
    `${i + 1}. [${c.hash}] ${c.subject} (${c.author}, ${c.date})`
  ).join('\n');

  const prompt = `다음 Git 커밋 로그를 분석하여 이번 버전의 주요 변경사항을 요약해주세요.

## 커밋 목록
${commitList}

## 요구사항
1. 커밋 메시지를 바탕으로 **주요 변경사항 3~5개**를 추출
2. 각 변경사항은 한 줄로 간결하게 작성 (한국어)
3. 버그 수정은 "수정:", 새 기능은 "추가:", 개선은 "개선:"으로 시작
4. 기술적인 용어는 그대로 사용
5. 중복되거나 사소한 변경사항은 제외

## 출력 형식
각 변경사항을 한 줄씩 출력하세요. (번호 없이)`;

  try {
    const response = execSync(
      `curl -s https://api.anthropic.com/v1/messages \\
        -H "x-api-key: ${ANTHROPIC_API_KEY}" \\
        -H "anthropic-version: 2023-06-01" \\
        -H "content-type: application/json" \\
        -d '${JSON.stringify({
          model: 'claude-3-5-haiku-20241022',
          max_tokens: 1000,
          messages: [
            { role: 'user', content: prompt }
          ]
        })}'`,
      { encoding: 'utf-8' }
    );

    const parsed = JSON.parse(response);
    const content = parsed.content?.[0]?.text || '';

    // 줄 단위로 파싱
    const suggestions = content
      .split('\n')
      .map(line => line.trim())
      .filter(line => line && !line.startsWith('#') && !line.startsWith('```'))
      .slice(0, 10); // 최대 10개

    console.log('  ✓ Claude 분석 완료\n');
    return suggestions;
  } catch (e) {
    console.log(`  ⚠️  Claude API 호출 실패: ${e.message}\n`);
    return null;
  }
}

/**
 * Display commit-based suggestions and get user input
 */
async function getChangesWithSuggestions(rl, commits) {
  console.log('\n' + '='.repeat(60));
  console.log('📋 변경사항 입력');
  console.log('='.repeat(60));

  let suggestions = null;

  // 커밋이 있고 Claude API가 있으면 자동으로 추천 먼저 표시
  if (commits.length > 0 && ANTHROPIC_API_KEY) {
    console.log('🤖 Claude AI가 커밋 로그를 분석 중...\n');
    suggestions = await getClaudeCommitSuggestions(commits);

    if (suggestions && suggestions.length > 0) {
      console.log('📋 Claude가 추천하는 changelog 변경사항:');
      console.log('-'.repeat(60));
      suggestions.forEach((s, i) => {
        console.log(`  ${i + 1}. ${s}`);
      });
      console.log('-'.repeat(60));

      console.log('\n위 내용을 기준으로 수정/추가/삭제할 수 있습니다.');
      console.log('빈 줄 입력 시 위 내용이 그대로 사용됩니다.\n');
    }
  }

  // 추천이 없으면 안내 메시지
  if (!suggestions || suggestions.length === 0) {
    console.log('변경사항을 자유롭게 입력하세요.');
    console.log('예시: WiFi 연결 안정성 개선, 이더넷 핀 플로팅 버그 수정');
    console.log('빈 줄을 입력하면 완료됩니다.\n');
  }

  // 사용자 입력 (추천 내용을 기본값으로 제공)
  const changes = [];
  let i = 1;

  while (true) {
    const defaultValue = suggestions && suggestions[i - 1] ? suggestions[i - 1] : '';
    const promptText = defaultValue ? `${i}. [${defaultValue}]` : `${i}.`;
    const change = await prompt(rl, promptText);

    if (!change.trim()) {
      if (defaultValue) {
        // 빈 입력이면 기본값 사용
        changes.push(defaultValue);
      }
      // 빈 입력이고 기본값도 없으면 종료
      if (!change.trim() && !defaultValue) {
        break;
      }
      i++;
      if (!change.trim() && defaultValue) {
        // 기본값을 추가하고 다음으로
        continue;
      }
      break;
    }

    changes.push(change.trim());
    i++;
  }

  // 추천만 사용하고 사용자 입력이 없으면 추천 반환
  if (changes.length === 0 && suggestions && suggestions.length > 0) {
    return suggestions;
  }

  return changes;
}

/**
 * Call Claude API to generate changelog from user input
 */
async function getClaudeChangelog(version, userChanges) {
  if (!ANTHROPIC_API_KEY) {
    return null;
  }

  console.log('\n🤖 Claude AI가 changelog를 작성 중...');

  const changesList = userChanges.map((c, i) => `${i + 1}. ${c}`).join('\n');

  const prompt = `다음 변경사항을 바탕으로 펌웨어 릴리스 노트(changelog)를 작성해주세요.

## 버전
${version}

## 사용자가 입력한 변경사항
${changesList}

## 요구사항
1. 한국어와 영어 두 가지 버전을 작성
2. 간결하고 명확한 기술 용어 사용
3. 각 변경사항을 명확한 문장으로 정리
4. JSON 형식으로 응답

## 출력 형식 (JSON만 출력)
\`\`\`json
{
  "ko": {
    "title": "버전 X.Y.Z",
    "changes": [
      "첫 번째 변경사항",
      "두 번째 변경사항"
    ]
  },
  "en": {
    "title": "Version X.Y.Z",
    "changes": [
      "First change description",
      "Second change description"
    ]
  }
}
\`\`\``;

  try {
    const response = execSync(
      `curl -s https://api.anthropic.com/v1/messages \\
        -H "x-api-key: ${ANTHROPIC_API_KEY}" \\
        -H "anthropic-version: 2023-06-01" \\
        -H "content-type: application/json" \\
        -d '${JSON.stringify({
          model: 'claude-3-5-haiku-20241022',
          max_tokens: 2000,
          messages: [
            { role: 'user', content: prompt }
          ]
        })}'`,
      { encoding: 'utf-8' }
    );

    const parsed = JSON.parse(response);
    const content = parsed.content?.[0]?.text || '';

    // JSON 추출
    const jsonMatch = content.match(/```json\n([\s\S]*?)\n```/) ||
                     content.match(/\{[\s\S]*\}/);

    if (jsonMatch) {
      const suggested = JSON.parse(jsonMatch[1] || jsonMatch[0]);
      console.log('  ✓ Changelog 작성 완료\n');
      return suggested;
    }

    return null;
  } catch (e) {
    console.log(`  ⚠️  Claude API 호출 실패: ${e.message}\n`);
    return null;
  }
}

/**
 * Manual changelog input
 */
async function getManualChangelog(rl, version, userChanges) {
  console.log('\n--- Changelog 수동 입력 ---');

  const koTitle = await prompt(rl, '한국어 제목', `버전 ${version}`);

  // 사용자 입력을 기반으로 변경사항 구성
  console.log('\n한국어 변경사항:');
  console.log('입력하신 내용을 기반으로 구성합니다. 그대로 사용하려면 엔터를 누르세요.');

  const koChanges = [];
  for (let i = 0; i < userChanges.length; i++) {
    const defaultChange = userChanges[i];
    const change = await prompt(rl, `  ${i + 1}`, defaultChange);
    koChanges.push(change || defaultChange);
  }

  // 영어
  const hasEn = await confirm(rl, '\n영어 번역 입력?');
  let enTitle = `Version ${version}`;
  let enChanges = [...koChanges];

  if (hasEn) {
    enTitle = await prompt(rl, '영어 제목', enTitle);

    console.log('\n영어 변경사항:');
    enChanges = [];
    for (let i = 0; i < koChanges.length; i++) {
      const change = await prompt(rl, `  ${i + 1}`);
      enChanges.push(change || koChanges[i]);
    }
  }

  return {
    ko: { title: koTitle, changes: koChanges },
    en: { title: enTitle, changes: enChanges }
  };
}

/**
 * Display changelog preview
 */
function displayChangelogPreview(data) {
  console.log('\n' + '='.repeat(60));
  console.log('📋 Changelog 미리보기');
  console.log('='.repeat(60));

  console.log(`\n[한국어] ${data.ko.title}`);
  data.ko.changes.forEach(c => console.log(`  - ${c}`));

  console.log(`\n[English] ${data.en.title}`);
  data.en.changes.forEach(c => console.log(`  - ${c}`));

  console.log('\n' + '='.repeat(60));
}

/**
 * Update platformio.ini
 */
function updatePlatformIOIni(newVersion) {
  const content = readFileSync(PLATFORMIO_INI_PATH, 'utf-8');
  const updated = content.replace(
    /-DFIRMWARE_VERSION=\\"[0-9.]+\\"/g,
    `-DFIRMWARE_VERSION=\\"${newVersion}\\"`
  );
  writeFileSync(PLATFORMIO_INI_PATH, updated, 'utf-8');
}

/**
 * Update app_types.h
 */
function updateAppTypes(newVersion) {
  const content = readFileSync(APP_TYPES_PATH, 'utf-8');
  const updated = content.replace(
    /#define FIRMWARE_VERSION "[0-9.]+"/g,
    `#define FIRMWARE_VERSION "${newVersion}"`
  );
  writeFileSync(APP_TYPES_PATH, updated, 'utf-8');
}

/**
 * Update web/package.json
 */
function updateWebPackage(newVersion) {
  const content = readFileSync(WEB_PACKAGE_PATH, 'utf-8');
  const pkg = JSON.parse(content);
  pkg.version = newVersion;
  writeFileSync(WEB_PACKAGE_PATH, JSON.stringify(pkg, null, 2), 'utf-8');
}

/**
 * Update state.js version (fallback)
 */
function updateStateJs(newVersion) {
  const stateJsPath = join(PROJECT_ROOT, 'web/src/js/modules/state.js');
  try {
    const content = readFileSync(stateJsPath, 'utf-8');
    const updated = content.replace(
      /version: '[0-9.]+'/,
      `version: '${newVersion}'`
    );
    writeFileSync(stateJsPath, updated, 'utf-8');
  } catch (e) {
    // Ignore if file not found
  }
}

/**
 * Load changelog
 */
function loadChangelog() {
  if (existsSync(CHANGELOG_PATH)) {
    const data = readFileSync(CHANGELOG_PATH, 'utf-8');
    return JSON.parse(data);
  }
  return { versions: [] };
}

/**
 * Save changelog
 */
function saveChangelog(changelog) {
  writeFileSync(CHANGELOG_PATH, JSON.stringify(changelog, null, 2), 'utf-8');
}

/**
 * Main function
 */
async function main() {
  const rl = createInterface();

  console.log('='.repeat(60));
  console.log('           Version Bump CLI v2.0                 ');
  console.log('='.repeat(60));

  // Display current versions
  const currentVersions = getCurrentVersions();
  const currentVersion = displayCurrentVersions(currentVersions);

  // Get new version (사용자 직접 입력, 추천 없음)
  console.log('\n새 버전을 입력하세요.');
  console.log('예: 2.4.2, 2.5.0, 3.0.0');
  const newVersion = await prompt(rl, '새 버전');

  if (!newVersion || !/^\d+\.\d+\.\d+$/.test(newVersion)) {
    console.log('\n❌ 올바른 버전 형식이 아닙니다. (X.Y.Z 형식 required)');
    rl.close();
    return;
  }

  console.log('\n' + '-'.repeat(60));
  console.log(`새 버전: ${newVersion}`);
  console.log('-'.repeat(60));

  const confirmVersion = await confirm(rl, '진행하시겠습니까?');
  if (!confirmVersion) {
    console.log('\n취소되었습니다.');
    rl.close();
    return;
  }

  // Get git commits for analysis
  const commits = getGitCommitsSince(currentVersion);

  if (commits.length > 0) {
    console.log(`\n📜 최근 ${commits.length}개의 커밋을 발견했습니다.`);
  }

  // Get changes from user (with Claude suggestions based on commits)
  const changes = await getChangesWithSuggestions(rl, commits);

  if (changes.length === 0) {
    console.log('\n⚠️  변경사항이 없습니다. changelog를 건너뜁니다.');

    // 버전만 업데이트
    console.log('\n' + '='.repeat(60));
    console.log('버전 파일 업데이트');
    console.log('='.repeat(60));

    if (existsSync(PLATFORMIO_INI_PATH)) {
      updatePlatformIOIni(newVersion);
      console.log('  ✓ platformio.ini');
    }

    if (existsSync(APP_TYPES_PATH)) {
      updateAppTypes(newVersion);
      console.log('  ✓ app_types.h');
    }

    if (existsSync(WEB_PACKAGE_PATH)) {
      updateWebPackage(newVersion);
      console.log('  ✓ web/package.json');
    }

    updateStateJs(newVersion);
    console.log('  ✓ web/src/js/modules/state.js');

    console.log('\n' + '='.repeat(60));
    console.log(`✅ 버전 ${newVersion}으로 업데이트 완료!`);
    console.log('='.repeat(60));
    rl.close();
    return;
  }

  // Generate changelog using Claude (based on user changes)
  let changelogData = null;
  const useClaude = ANTHROPIC_API_KEY && await confirm(rl, '\nClaude AI에게 changelog 작성을 요청하시겠습니까?');

  if (useClaude) {
    changelogData = await getClaudeChangelog(newVersion, changes);

    if (changelogData) {
      displayChangelogPreview(changelogData);
      const acceptClaude = await confirm(rl, '\nClaude가 작성한 changelog를 사용하시겠습니까?');

      if (!acceptClaude) {
        changelogData = null;
      }
    }
  }

  // Manual input if Claude declined or failed
  if (!changelogData) {
    changelogData = await getManualChangelog(rl, newVersion, changes);
  }

  // Add to changelog
  const changelog = loadChangelog();

  // Remove existing version if present
  changelog.versions = changelog.versions.filter(v => v.version !== newVersion);

  // Add new version
  changelog.versions.unshift({
    version: newVersion,
    date: new Date().toISOString().split('T')[0],
    ko: changelogData.ko,
    en: changelogData.en
  });

  saveChangelog(changelog);
  console.log('\n✅ changelog.json 업데이트 완료');

  // Update all version files
  console.log('\n' + '='.repeat(60));
  console.log('버전 파일 업데이트');
  console.log('='.repeat(60));

  if (existsSync(PLATFORMIO_INI_PATH)) {
    updatePlatformIOIni(newVersion);
    console.log('  ✓ platformio.ini');
  }

  if (existsSync(APP_TYPES_PATH)) {
    updateAppTypes(newVersion);
    console.log('  ✓ app_types.h');
  }

  if (existsSync(WEB_PACKAGE_PATH)) {
    updateWebPackage(newVersion);
    console.log('  ✓ web/package.json');
  }

  updateStateJs(newVersion);
  console.log('  ✓ web/src/js/modules/state.js');

  console.log('\n' + '='.repeat(60));
  console.log(`✅ 버전 ${newVersion}으로 업데이트 완료!`);
  console.log('='.repeat(60));

  console.log('\n다음 단계:');
  console.log('  1. 웹 UI 빌드: cd web && npm run deploy');
  console.log('  2. 펌웨어 빌드: pio run -e eora_s3_tx --target upload');
  console.log('');

  rl.close();
}

// Run
main().catch(error => {
  console.error('Error:', error);
  process.exit(1);
});
