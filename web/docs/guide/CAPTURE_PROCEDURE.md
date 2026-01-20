# 스크린샷 캡처 절차

## 개요

이 문서는 Tally Node Web UI의 스크린샷을 자동으로 캡처하기 위한 절차를 설명합니다.

## 사전 요구사항

### 1. 개발 환경 설정

```bash
# 프로젝트 디렉토리로 이동
cd /home/prod/tally-node/web

# 의존성 설치
npm install

# 개발 서버 시작
npm run dev
```

### 2. Playwright 설치

```bash
# Playwright 설치
npm install -D @playwright/test

# 브라우저 설치
npx playwright install chromium
```

## 자동화 캡처 절차

### STEP 1: UI 상태 준비

캡처 전 UI가 올바른 상태로 표시되도록 데이터를 설정합니다.

```javascript
// mock-data.js
export const mockSystemData = {
  battery: 85,
  temperature: 42,
  uptime: 3600,
  deviceId: "XXXX",  // 마스킹 대상
  voltage: 12.4,
  version: "1.0.0"
};

export const mockNetworkData = {
  apEnabled: true,
  apSsid: "Tally-Node-XXXX",  // 마스킹 대상
  apIp: "192.168.4.1",  // 마스킹 대상
  wifiConnected: true,
  wifiSsid: "Home-WiFi-XXXX",  // 마스킹 대상
  wifiIp: "192.168.1.100",  // 마스킹 대상
  ethConnected: true,
  ethIp: "192.168.1.101"  // 마스킹 대상
};
```

### STEP 2: Playwright 스크립트 작성

```typescript
// screenshots/capture-dashboard.ts
import { test, expect, Page } from '@playwright/test';

async function captureDashboard(page: Page) {
  // 대시보드 페이지로 이동
  await page.goto('http://localhost:8080/#dashboard');

  // 페이지 로드 대기
  await page.waitForLoadState('networkidle');
  await page.waitForTimeout(1000);

  // 전체 페이지 캡처
  await page.screenshot({
    path: 'docs/screenshots/dashboard/dashboard-main-desktop-default-01.png',
    fullPage: true
  });
}

test('capture dashboard screenshots', async ({ page }) => {
  await captureDashboard(page);
});
```

### STEP 3: 캡처 실행

```bash
# 전체 캡처 실행
npm run screenshot:all

# 특정 페이지 캡처
npm run screenshot:dashboard
npm run screenshot:network
npm run screenshot:devices
```

## 수동 캡처 절차

개발 서버가 실행 중이지 않은 경우 수동 캡처를 사용합니다.

### STEP 1: 개발 서버 시작

```bash
cd /home/prod/tally-node/web
npm run dev
```

### STEP 2: 브라우저에서 캡처

1. Chrome 개발자 도구 열기 (F12)
2. Device Toolbar 열기 (Ctrl+Shift+M / Cmd+Shift+M)
3. 디바이스 선택 (Desktop / Mobile)
4. 페이지 탐색
5. 스크린샷 캡처 (Ctrl+Shift+P / Cmd+Shift+P → "Capture full size screenshot")

### STEP 3: 파일 저장

캡처한 파일을 적절한 디렉토리에 저장합니다:

```
web/docs/screenshots/{page}/
└── {page}-{view}-{device}-{state}-{variant}.png
```

## 개인정보 마스킹 절차

### 자동 마스킹

스크립트를 사용하여 자동으로 개인정보를 마스킹합니다.

```typescript
// screenshots/mask-privacy.ts
import sharp from 'sharp';

async function maskSensitiveData(imagePath: string) {
  const image = sharp(imagePath);
  const metadata = await image.metadata();

  // 마스킹할 영역 정의
  const masks = [
    { left: 100, top: 50, width: 150, height: 30 },  // IP 주소
    { left: 300, top: 100, width: 100, height: 30 }, // 디바이스 ID
  ];

  // 각 영역에 블러 효과 적용
  for (const mask of masks) {
    // 블러 효과 적용 로직
  }

  await image.png().toFile(imagePath);
}
```

### 수동 마스킹

1. Figma 또는 Photopea에서 이미지 열기
2. 레이어 추가
3. 직사각형 선택 도구로 마스킹 영역 선택
4. 블러 효과 또는 색상 레이어 적용
5. PNG로 내보내기

## 주석 추가 절차

### 1단계: Figma 사용

1. Figma에서 새 프로젝트 생성
2. 스크린샷 이미지 임포트
3. 주석 레이어 추가
4. 텍스트, 화살표, 번호 매기기 추가
5. PNG로 내보내기

### 2단계: Photopea 사용 (무료 대안)

1. [Photopea](https://www.photopea.com/) 열기
2. File → Open으로 이미지 열기
3. 레이어 추가
4. Shape 도구로 화살표 추가
5. Text 도구로 주석 추가
6. File → Export As → PNG

### 3단계: GIMP 사용 (오픈소스)

1. GIMP에서 이미지 열기
2. 레이어 → 새 레이어
3. 그리기 도구로 화살표 추가
4. 텍스트 도구로 주석 추가
5. File → Export As → PNG

## 품질 검증 절차

### 1. 해상도 확인

```bash
# ImageMagick 사용
identify -format "%w x %h" screenshot.png

# 예상 출력: 3840 x 2160 (2x scale)
```

### 2. 파일 크기 확인

```bash
# 파일 크기 확인 (500KB 미만 권장)
ls -lh screenshot.png
```

### 3. 개인정보 마스킹 확인

```bash
# 스크립트로 마스킹 확인
npm run test:privacy
```

### 4. 시각적 검증

- [ ] 이미지가 선명한가?
- [ ] 색상이 정확한가?
- [ ] 모든 텍스트가 읽기 쉬운가?
- [ ] 주석이 명확한가?

## 문서 통합 절차

### 1. 이미지 파일 복사

```bash
# 최종 이미지를 문서 디렉토리에 복사
cp screenshots/final/*.png docs/screenshots/
```

### 2. Markdown 업데이트

```markdown
## 대시보드 페이지

### 개요
대시보드는 시스템의 전체 상태를 한눈에 확인할 수 있는 메인 페이지입니다.

![대시보드 메인 화면](../screenshots/dashboard/dashboard-main-desktop-default-01.png)

*그림 1: 대시보드 메인 화면 (Desktop)*
```

### 3. 링크 검증

```bash
# 모든 이미지 링크 검증
npm run test:links
```

## 문제 해결

### 스크린샷이 흐릿한 경우

**원인**: 브라우저 확대/축소 설정

**해결**:
1. 브라우저 확대/축소를 100%로 설정
2. Playwright에서 `viewport` 설정 확인

### 이미지가 너무 큰 경우

**원인**: 2x 스케일 미적용

**해결**:
```bash
# TinyPNG로 압축
https://tinypng.com/

# 또는 ImageMagick으로 최적화
convert input.png -quality 85 output.png
```

### 개인정보가 노출된 경우

**원인**: 마스킹 누락

**해결**:
1. 마스킹 스크립트 재실행
2. 수동으로 마스킹
3. 품질 검증 재실행

## 자동화 스크립트

### 전체 캡처 스크립트

```bash
#!/bin/bash
# capture-all.sh

npm run dev &
DEV_PID=$!

sleep 5

npx playwright test screenshots/

kill $DEV_PID

echo "Screenshots captured successfully!"
```

### 마스킹 스크립트

```bash
#!/bin/bash
# mask-all.sh

for file in docs/screenshots/**/*.png; do
  npm run privacy:mask "$file"
done

echo "Privacy masking completed!"
```

---

**문서 버전**: 1.0.0
**마지막 업데이트**: 2026-01-19
**유지 관리자**: MoAI-ADK Documentation Team
