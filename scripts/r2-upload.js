#!/usr/bin/env node

/**
 * Cloudflare R2 Firmware Upload Script
 *
 * Uploads ESP32 firmware binaries to Cloudflare R2 storage.
 *
 * Usage: npm run r2-upload
 *
 * Environment variables required in .env:
 * - R2_ACCOUNT_ID: Cloudflare Account ID
 * - R2_ACCESS_KEY_ID: R2 Access Key ID
 * - R2_SECRET_ACCESS_KEY: R2 Secret Access Key
 * - R2_BUCKET: R2 Bucket name
 *
 * Upload path structure:
 * eora_s3/changelog.json                    # 전체 변경 로그
 * eora_s3/latest.json                       # 최신 버전 정보
 * eora_s3/{version}/metadata.json           # 버전별 메타데이터
 * eora_s3/{version}/{tx|rx}/{firmware,bootloader,partitions}.bin
 */

const { S3Client, PutObjectCommand } = require('@aws-sdk/client-s3');
const { readFileSync, existsSync, statSync, writeFileSync } = require('fs');
const { join } = require('path');
const readline = require('readline');
const { execSync } = require('child_process');
require('dotenv').config({ path: join(__dirname, '..', '.env') });

// Import version check functions
const { checkAllVersions } = require('./version-check.js');

// Configuration from environment variables
const R2_ACCOUNT_ID = process.env.R2_ACCOUNT_ID;
const R2_ACCESS_KEY_ID = process.env.R2_ACCESS_KEY_ID;
const R2_SECRET_ACCESS_KEY = process.env.R2_SECRET_ACCESS_KEY;
const R2_BUCKET = process.env.R2_BUCKET;

// R2 path prefix
const R2_PREFIX = 'eora_s3';

// Global VERSION variable (will be set in main)
let VERSION;

// Board configurations
const BASE_BOARD = 'eora_s3';
const BOARDS = [
  {
    name: 'TX',
    subfolder: 'tx',
    buildEnv: 'eora_s3_tx'
  },
  {
    name: 'RX',
    subfolder: 'rx',
    buildEnv: 'eora_s3_rx'
  }
];

// Binary files to upload for each board
const BINARY_FILES = [
  { name: 'firmware', filename: 'firmware.bin' },
  { name: 'bootloader', filename: 'bootloader.bin' },
  { name: 'partitions', filename: 'partitions.bin' }
];

/**
 * Validate required environment variables
 */
function validateEnvironment() {
  const required = [
    { var: R2_ACCOUNT_ID, name: 'R2_ACCOUNT_ID' },
    { var: R2_ACCESS_KEY_ID, name: 'R2_ACCESS_KEY_ID' },
    { var: R2_SECRET_ACCESS_KEY, name: 'R2_SECRET_ACCESS_KEY' },
    { var: R2_BUCKET, name: 'R2_BUCKET' }
  ];

  const missing = required.filter(v => !v.var);
  if (missing.length > 0) {
    console.error('Error: Missing required environment variables:');
    missing.forEach(v => console.error(`  - ${v.name}`));
    console.error('\nPlease create a .env file with the following variables:');
    console.error('  R2_ACCOUNT_ID=your_account_id');
    console.error('  R2_ACCESS_KEY_ID=your_access_key_id');
    console.error('  R2_SECRET_ACCESS_KEY=your_secret_access_key');
    console.error('  R2_BUCKET=tally-node');
    process.exit(1);
  }
}

/**
 * Initialize S3 client for Cloudflare R2
 */
function createS3Client() {
  return new S3Client({
    region: 'auto',
    endpoint: `https://${R2_ACCOUNT_ID}.r2.cloudflarestorage.com`,
    credentials: {
      accessKeyId: R2_ACCESS_KEY_ID,
      secretAccessKey: R2_SECRET_ACCESS_KEY
    }
  });
}

/**
 * Upload a single file to R2
 */
async function uploadFile(s3Client, key, data, contentType = 'application/octet-stream', metadata = {}) {
  console.log(`📤 Uploading to R2...`);
  console.log(`   Key: ${key}`);

  const startTime = Date.now();

  try {
    const command = new PutObjectCommand({
      Bucket: R2_BUCKET,
      Key: key,
      Body: data,
      ContentType: contentType,
      Metadata: metadata
    });

    await s3Client.send(command);

    const duration = ((Date.now() - startTime) / 1000).toFixed(2);
    console.log(`✅ Uploaded successfully! (${duration}s)`);
    console.log(`   URL: https://${R2_ACCOUNT_ID}.r2.cloudflarestorage.com/${R2_BUCKET}/${key}`);

    return { success: true, key, duration };
  } catch (error) {
    console.error(`❌ Failed to upload: ${error.message}`);
    return { success: false, key, error: error.message };
  }
}

/**
 * Upload a single binary file to R2
 */
async function uploadBinaryFile(s3Client, board, binaryFile) {
  const filePath = join(__dirname, '..', '.pio', 'build', board.buildEnv, binaryFile.filename);

  // Check if file exists
  if (!existsSync(filePath)) {
    console.log(`⚠️  Skipping ${board.name}/${binaryFile.filename}: file not found at ${filePath}`);
    return { success: false, skipped: true, board: board.name, file: binaryFile.filename };
  }

  // Get file size for progress display
  const stats = statSync(filePath);
  const fileSize = stats.size;
  const fileSizeKB = (fileSize / 1024).toFixed(2);

  // Read file
  console.log(`📦 Reading ${board.name}/${binaryFile.filename} (${fileSizeKB} KB)...`);
  const fileData = readFileSync(filePath);

  // Construct R2 key: eora_s3/{version}/{tx|rx}/{filename}
  const key = `${R2_PREFIX}/${VERSION}/${board.subfolder}/${binaryFile.filename}`;

  const result = await uploadFile(s3Client, key, fileData, 'application/octet-stream', {
    version: VERSION,
    board: board.name,
    baseBoard: BASE_BOARD,
    subfolder: board.subfolder,
    binaryType: binaryFile.name,
    uploadedAt: new Date().toISOString()
  });

  return { ...result, board: board.name, file: binaryFile.filename, size: fileSizeKB };
}

/**
 * Generate metadata.json for current version
 */
function generateMetadata() {
  const metadata = {
    version: VERSION,
    date: new Date().toISOString().split('T')[0],
    boards: {}
  };

  for (const board of BOARDS) {
    const boardData = {
      buildEnv: board.buildEnv,
      binaries: []
    };

    for (const binaryFile of BINARY_FILES) {
      const filePath = join(__dirname, '..', '.pio', 'build', board.buildEnv, binaryFile.filename);
      if (existsSync(filePath)) {
        const stats = statSync(filePath);
        boardData.binaries.push({
          name: binaryFile.name,
          filename: binaryFile.filename,
          size: stats.size,
          sha256: null // TODO: Calculate SHA256 if needed
        });
      }
    }

    metadata.boards[board.subfolder] = boardData;
  }

  return metadata;
}

/**
 * Generate latest.json pointing to current version
 */
function generateLatest() {
  return {
    version: VERSION,
    date: new Date().toISOString().split('T')[0],
    updated_at: new Date().toISOString(),
    files: {
      tx: {
        firmware: `${R2_PREFIX}/${VERSION}/tx/firmware.bin`,
        bootloader: `${R2_PREFIX}/${VERSION}/tx/bootloader.bin`,
        partitions: `${R2_PREFIX}/${VERSION}/tx/partitions.bin`
      },
      rx: {
        firmware: `${R2_PREFIX}/${VERSION}/rx/firmware.bin`,
        bootloader: `${R2_PREFIX}/${VERSION}/rx/bootloader.bin`,
        partitions: `${R2_PREFIX}/${VERSION}/rx/partitions.bin`
      },
      metadata: `${R2_PREFIX}/${VERSION}/metadata.json`
    }
  };
}
/**
 * Check if version exists in changelog
 */
function hasVersionInChangelog(changelog) {
  return changelog.versions.some(v => v.version === VERSION);
}

/**
 * Prompt user for confirmation
 */
function confirm(message) {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  return new Promise((resolve) => {
    rl.question(`${message} (y/n): `, (answer) => {
      rl.close();
      resolve(answer.toLowerCase() === 'y' || answer.toLowerCase() === 'yes');
    });
  });
}

/**
 * Generate changelog entry using Claude CLI
 */
function generateChangelogWithClaude(changes) {
  const today = new Date().toISOString().split('T')[0];

  const prompt = `작업: 펌웨어 changelog 생성 (한글 + 영어)

=== 변경사항 ===
${changes || '없음'}

=== 지시사항 ===
1. 한글(ko)과 영어(en)双语로 작성
2. title은 변경사항을 요약한 짧은 문장
3. changes는 기능적 핵심만 3개 이내로 작성 (기술적 세부사항은 생략)
4. 관련 기능은 하나로 통합 (예: "IP 검사 강화 및 자동 전환 추가")
5. 출력은 JSON 객체만 반환 (코드 블록 없이)

버전: ${VERSION}
날짜: ${today}

JSON 형식:
{
  "version": "${VERSION}",
  "date": "${today}",
  "ko": { "title": "...", "changes": [...] },
  "en": { "title": "...", "changes": [...] }
}`;

  try {
    const result = execSync(`echo '${prompt.replace(/'/g, "'\\''")}' | claude`, {
      encoding: 'utf-8',
      stdio: ['pipe', 'pipe', 'pipe']
    });

    return result;
  } catch (error) {
    console.error('Claude CLI 호출 실패:', error.message);
    return null;
  }
}

/**
 * Upload changelog.json to R2
 */
async function uploadChangelog(s3Client) {
  const changelogPath = join(__dirname, '..', 'changelog.json');

  if (!existsSync(changelogPath)) {
    console.log('⚠️  Skipping changelog.json: file not found');
    console.log('   Run "npm run changelog-add" to create changelog.json');
    return { success: false, skipped: true, file: 'changelog.json', reason: 'not_found' };
  }

  let changelogData = readFileSync(changelogPath, 'utf-8');
  let changelog = JSON.parse(changelogData);

  // Check if version already exists
  if (hasVersionInChangelog(changelog)) {
    console.log(`⚠️  Version ${VERSION} already exists in changelog.json`);
    const shouldUpload = await confirm('Do you want to upload anyway?');

    if (!shouldUpload) {
      console.log('Skipping changelog.json upload');
      return { success: false, skipped: true, file: 'changelog.json', reason: 'user_cancelled' };
    }
  } else {
    // Version not found, prompt user for changelog
    console.log(`\n${'='.repeat(60)}`);
    console.log(`Changelog 작성 for version ${VERSION}`);
    console.log(`${'='.repeat(60)}`);

    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });

    // 변경사항 입력 (TX/RX 구분 없음)
    const changes = await new Promise((resolve) => {
      const lines = [];
      const ask = () => {
        rl.question('변경사항 (빈 줄로 완료): ', (line) => {
          if (!line) {
            resolve(lines.join('\n'));
          } else {
            lines.push(line);
            ask();
          }
        });
      };
      ask();
    });

    rl.close();

    // Generate changelog using Claude CLI
    console.log('\n🤖 Claude로 changelog 생성 중...');

    const claudeResult = generateChangelogWithClaude(changes);

    if (claudeResult) {
      try {
        // Parse Claude response
        let jsonMatch = claudeResult.match(/\{[\s\S]*\}/);
        if (jsonMatch) {
          let newEntry = JSON.parse(jsonMatch[0]);

          // Changelog 확인 및 수정 루프
          while (true) {
            console.log('\n📝 생성된 changelog:');
            console.log(JSON.stringify(newEntry, null, 2));

            const action = await new Promise((resolve) => {
              const rlConfirm = readline.createInterface({
                input: process.stdin,
                output: process.stdout
              });
              rlConfirm.question('이 changelog를 추가하시겠습니까? (y: 추가, n: 취소, m: 수정 의견 입력): ', (answer) => {
                rlConfirm.close();
                resolve(answer.toLowerCase().trim());
              });
            });

            if (action === 'y' || action === 'yes') {
              break; // 추가 진행
            } else if (action === 'n' || action === 'no') {
              console.log('⚠️  Changelog 추가가 취소되었습니다.');
              return { success: false, skipped: true, file: 'changelog.json', reason: 'user_cancelled' };
            } else if (action === 'm') {
              // 수정 의견 입력
              const revision = await new Promise((resolve) => {
                const rlRev = readline.createInterface({
                  input: process.stdin,
                  output: process.stdout
                });
                rlRev.question('수정 의견을 입력하세요: ', (answer) => {
                  rlRev.close();
                  resolve(answer);
                });
              });

              if (!revision.trim()) {
                console.log('수정 의견이 비어있어서 다시 시도합니다.');
                continue;
              }

              // Claude로 수정 요청
              console.log('\n🤖 Claude로 changelog 수정 중...');

              const revisionPrompt = `작업: 기존 changelog 수정

=== 기존 changelog ===
${JSON.stringify(newEntry, null, 2)}

=== 수정 의견 ===
${revision}

=== 지시사항 ===
1. 수정 의견을 반영하여 changelog를 수정하세요
2. 한글(ko)과 영어(en)双语로 작성
3. 출력은 JSON 객체만 반환 (코드 블록 없이)`;

              try {
                const revisionResult = execSync(`echo '${revisionPrompt.replace(/'/g, "'\\''")}' | claude`, {
                  encoding: 'utf-8',
                  stdio: ['pipe', 'pipe', 'pipe']
                });

                const jsonMatch2 = revisionResult.match(/\{[\s\S]*\}/);
                if (jsonMatch2) {
                  newEntry = JSON.parse(jsonMatch2[0]);
                  console.log('✅ changelog가 수정되었습니다.');
                } else {
                  console.log('⚠️  JSON 파싱 실패, 다시 시도하세요.');
                }
              } catch (error) {
                console.error(`❌ 수정 실패: ${error.message}`);
                console.log('다시 시도하세요.');
              }
              // 루프 계속
            } else {
              console.log('y, n, m 중 하나를 입력하세요.');
            }
          }

          // 추가 진행
          changelog.versions.push(newEntry);

          // Sort by date (newest first), then by version (newest first)
          changelog.versions.sort((a, b) => {
            const dateCompare = new Date(b.date) - new Date(a.date);
            if (dateCompare !== 0) return dateCompare;
            // Same date: sort by version (newest first)
            return b.version.localeCompare(a.version);
          });

          // Update changelogData
          changelogData = JSON.stringify(changelog, null, 2);

          // Save to file
          writeFileSync(changelogPath, changelogData, 'utf-8');

          console.log(`✅ Added version ${VERSION} to changelog.json`);
        } else {
          throw new Error('JSON 파싱 실패');
        }
      } catch (error) {
        console.error('❌ Changelog 생성 실패:', error.message);
        console.log('Claude 응답:', claudeResult);
        return { success: false, skipped: true, file: 'changelog.json', reason: 'claude_failed' };
      }
    } else {
      console.log('⚠️  Changelog를 추가하지 않고 계속합니다.');
    }
  }

  const key = `${R2_PREFIX}/changelog.json`;

  return await uploadFile(s3Client, key, changelogData, 'application/json', {
    type: 'changelog',
    version: VERSION,
    updated_at: new Date().toISOString()
  });
}

/**
 * Upload metadata.json to R2
 */
async function uploadMetadata(s3Client) {
  const metadata = generateMetadata();
  const metadataJson = JSON.stringify(metadata, null, 2);
  const key = `${R2_PREFIX}/${VERSION}/metadata.json`;

  // Also save local copy
  const localMetadataPath = join(__dirname, '..', '.pio', 'build', `metadata-${VERSION}.json`);
  writeFileSync(localMetadataPath, metadataJson, 'utf-8');

  return await uploadFile(s3Client, key, metadataJson, 'application/json', {
    version: VERSION,
    type: 'metadata'
  });
}

/**
 * Upload latest.json to R2
 */
async function uploadLatest(s3Client) {
  const latest = generateLatest();
  const latestJson = JSON.stringify(latest, null, 2);
  const key = `${R2_PREFIX}/latest.json`;

  return await uploadFile(s3Client, key, latestJson, 'application/json', {
    type: 'latest',
    version: VERSION
  });
}

/**
 * Update changelog using Claude CLI
 */
async function updateChangelog(targetVersion, changelogVersion) {
  console.log(`\n⚠️  changelog.json만 버전이 다릅니다 (${changelogVersion} → ${targetVersion})`);
  console.log('\nchangelog.json를 자동으로 업데이트하시겠습니까?');

  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });

  const shouldUpdate = await new Promise((resolve) => {
    rl.question('업데이트하려면 y, 건너뛰려면 n: ', (answer) => {
      rl.close();
      resolve(answer.toLowerCase() === 'y' || answer.toLowerCase() === 'yes');
    });
  });

  if (!shouldUpdate) {
    return false;
  }

  // 출력 최근 10개 git 로그
  console.log('\n' + '='.repeat(60));
  console.log('최근 Git 커밋 로그 (10개)');
  console.log('='.repeat(60));
  try {
    const gitLog = execSync('git log -10 --oneline --no-decorate', {
      encoding: 'utf-8',
      cwd: join(__dirname, '..')
    });
    console.log(gitLog);
  } catch (error) {
    console.log('(Git 로그를 가져올 수 없습니다)');
  }
  console.log('='.repeat(60));

  console.log('\n변경사항을 입력하세요 (빈 줄로 완료):');

  // 변경사항 입력 (TX/RX 구분 없음)
  const changes = await new Promise((resolve) => {
    const lines = [];
    const rl2 = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    const ask = () => {
      rl2.question('변경사항 (빈 줄로 완료): ', (line) => {
        if (!line) {
          rl2.close();
          resolve(lines.join('\n'));
        } else {
          lines.push(line);
          ask();
        }
      });
    };
    ask();
  });

  // Claude CLI로 changelog 생성
  console.log('\n🤖 Claude로 changelog 생성 중...');

  const today = new Date().toISOString().split('T')[0];
  const prompt = `작업: 펌웨어 changelog 생성 (한글 + 영어)

=== 변경사항 ===
${changes || '없음'}

=== 지시사항 ===
1. 한글(ko)과 영어(en)双语로 작성
2. title은 변경사항을 요약한 짧은 문장
3. changes는 기능적 핵심만 3개 이내로 작성 (기술적 세부사항은 생략)
4. 관련 기능은 하나로 통합 (예: "IP 검사 강화 및 자동 전환 추가")
5. 출력은 JSON 객체만 반환 (코드 블록 없이)

버전: ${targetVersion}
날짜: ${today}

JSON 형식:
{
  "version": "${targetVersion}",
  "date": "${today}",
  "ko": { "title": "...", "changes": [...] },
  "en": { "title": "...", "changes": [...] }
}`;

  try {
    const claudeResult = execSync(`echo '${prompt.replace(/'/g, "'\\''")}' | claude`, {
      encoding: 'utf-8',
      stdio: ['pipe', 'pipe', 'pipe']
    });

    // Parse Claude response
    let jsonMatch = claudeResult.match(/\{[\s\S]*\}/);
    if (!jsonMatch) {
      throw new Error('JSON 파싱 실패');
    }

    let newEntry = JSON.parse(jsonMatch[0]);

    // Changelog 확인 및 수정 루프
    while (true) {
      console.log('\n📝 생성된 changelog:');
      console.log(JSON.stringify(newEntry, null, 2));

      const action = await new Promise((resolve) => {
        const rl4 = readline.createInterface({
          input: process.stdin,
          output: process.stdout
        });
        rl4.question('이 changelog를 추가하시겠습니까? (y: 추가, n: 취소, m: 수정 의견 입력): ', (answer) => {
          rl4.close();
          resolve(answer.toLowerCase().trim());
        });
      });

      if (action === 'y' || action === 'yes') {
        break; // 추가 진행
      } else if (action === 'n' || action === 'no') {
        return false; // 취소
      } else if (action === 'm') {
        // 수정 의견 입력
        const revision = await new Promise((resolve) => {
          const rl5 = readline.createInterface({
            input: process.stdin,
            output: process.stdout
          });
          rl5.question('수정 의견을 입력하세요: ', (answer) => {
            rl5.close();
            resolve(answer);
          });
        });

        if (!revision.trim()) {
          console.log('수정 의견이 비어있어서 다시 시도합니다.');
          continue;
        }

        // Claude로 수정 요청
        console.log('\n🤖 Claude로 changelog 수정 중...');

        const revisionPrompt = `작업: 기존 changelog 수정

=== 기존 changelog ===
${JSON.stringify(newEntry, null, 2)}

=== 수정 의견 ===
${revision}

=== 지시사항 ===
1. 수정 의견을 반영하여 changelog를 수정하세요
2. 한글(ko)과 영어(en)双语로 작성
3. 출력은 JSON 객체만 반환 (코드 블록 없이)`;

        try {
          const revisionResult = execSync(`echo '${revisionPrompt.replace(/'/g, "'\\''")}' | claude`, {
            encoding: 'utf-8',
            stdio: ['pipe', 'pipe', 'pipe']
          });

          const jsonMatch2 = revisionResult.match(/\{[\s\S]*\}/);
          if (jsonMatch2) {
            newEntry = JSON.parse(jsonMatch2[0]);
            console.log('✅ changelog가 수정되었습니다.');
          } else {
            console.log('⚠️  JSON 파싱 실패, 다시 시도하세요.');
          }
        } catch (error) {
          console.error(`❌ 수정 실패: ${error.message}`);
          console.log('다시 시도하세요.');
        }
        // 루프 계속
      } else {
        console.log('y, n, m 중 하나를 입력하세요.');
      }
    }

    // Read changelog.json
    const changelogPath = join(__dirname, '..', 'changelog.json');
    const changelogData = readFileSync(changelogPath, 'utf-8');
    const changelog = JSON.parse(changelogData);

    // Add new entry
    changelog.versions.push(newEntry);

    // Sort by date (newest first), then by version (newest first)
    changelog.versions.sort((a, b) => {
      const dateCompare = new Date(b.date) - new Date(a.date);
      if (dateCompare !== 0) return dateCompare;
      // Same date: sort by version (newest first)
      return b.version.localeCompare(a.version);
    });

    // Save
    writeFileSync(changelogPath, JSON.stringify(changelog, null, 2), 'utf-8');

    console.log(`\n✅ changelog.json에 버전 ${targetVersion}이 추가되었습니다.`);
    return true;
  } catch (error) {
    console.error(`\n❌ Changelog 생성 실패: ${error.message}`);
    return false;
  }
}

/**
 * Main upload function
 */
async function main() {
  // Check all versions first
  let versionResult;
  try {
    versionResult = checkAllVersions();
    VERSION = versionResult.version;
  } catch (error) {
    console.error(`\n❌ 버전 확인 실패: ${error.message}`);
    console.log('먼저 "npm run version"으로 버전을 확인하세요.');
    process.exit(1);
  }

  // changelog만 다른 경우 업데이트 수행
  if (versionResult.changelogOnlyMismatch) {
    const updated = await updateChangelog(VERSION, versionResult.changelogVersion);
    if (!updated) {
      console.log('\n업데이트가 취소되었습니다.');
      process.exit(1);
    }
  }

  console.log(`\n📋 펌웨어 버전 확인 완료: ${VERSION}`);

  console.log('='.repeat(60));
  console.log('Cloudflare R2 Firmware Upload');
  console.log('='.repeat(60));
  console.log(`Version: ${VERSION}`);
  console.log(`Timestamp: ${new Date().toISOString()}`);
  console.log(`R2 Prefix: ${R2_PREFIX}`);
  console.log('');

  // Validate environment
  validateEnvironment();

  // Create S3 client
  const s3Client = createS3Client();

  const results = [];

  // 1. Upload changelog.json
  console.log(`\n${'─'.repeat(60)}`);
  console.log('Step 1: Uploading changelog.json');
  console.log(`${'─'.repeat(60)}`);
  const changelogResult = await uploadChangelog(s3Client);
  results.push({ type: 'changelog', ...changelogResult });
  console.log('');

  // 2. Upload metadata.json
  console.log(`\n${'─'.repeat(60)}`);
  console.log('Step 2: Uploading metadata.json');
  console.log(`${'─'.repeat(60)}`);
  const metadataResult = await uploadMetadata(s3Client);
  results.push({ type: 'metadata', ...metadataResult });
  console.log('');

  // 3. Upload latest.json
  console.log(`\n${'─'.repeat(60)}`);
  console.log('Step 3: Uploading latest.json');
  console.log(`${'─'.repeat(60)}`);
  const latestResult = await uploadLatest(s3Client);
  results.push({ type: 'latest', ...latestResult });
  console.log('');

  // 4. Upload binary files
  console.log(`\n${'─'.repeat(60)}`);
  console.log('Step 4: Uploading firmware binaries');
  console.log(`${'─'.repeat(60)}`);

  for (const board of BOARDS) {
    console.log(`\nProcessing ${board.name} board...`);

    for (const binaryFile of BINARY_FILES) {
      const result = await uploadBinaryFile(s3Client, board, binaryFile);
      results.push({ type: 'binary', ...result });
      console.log('');
    }
  }

  // Print summary
  console.log('='.repeat(60));
  console.log('Upload Summary');
  console.log('='.repeat(60));

  const successful = results.filter(r => r.success);
  const skipped = results.filter(r => r.skipped);
  const failed = results.filter(r => !r.success && !r.skipped);

  console.log(`✅ Successful: ${successful.length}`);
  console.log(`⚠️  Skipped: ${skipped.length}`);
  console.log(`❌ Failed: ${failed.length}`);

  if (successful.length > 0) {
    console.log('\nUploaded files:');
    successful.forEach(r => {
      if (r.type === 'binary') {
        console.log(`  - ${r.board}/${r.file}: ${r.key} (${r.size} KB, ${r.duration}s)`);
      } else {
        console.log(`  - ${r.type}: ${r.key} (${r.duration}s)`);
      }
    });
  }

  if (skipped.length > 0) {
    console.log('\nSkipped files:');
    skipped.forEach(r => {
      if (r.type === 'binary') {
        console.log(`  - ${r.board}/${r.file}: file not found`);
      } else {
        console.log(`  - ${r.file}: file not found`);
      }
    });
  }

  if (failed.length > 0) {
    console.log('\nFailed uploads:');
    failed.forEach(r => {
      console.log(`  - ${r.key}: ${r.error}`);
    });
    process.exit(1);
  }

  console.log('\n✨ All uploads completed successfully!');
}

// Run the script
main().catch(error => {
  console.error('Fatal error:', error);
  process.exit(1);
});
