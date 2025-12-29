# Tally Web UI

Tally Node용 웹 인터페이스입니다.

## 개발

```bash
# 개발 서버 시작 (http://localhost:8080)
npm run dev

# 빌드 (dist/ 폴더 생성)
npm run build

# ESP32용 임베디드 파일 생성
npm run embed

# ESP32 컴포넌트로 복사
npm run copy

# 전체 배포 (빌드 + 임베드 + 복사)
npm run deploy
```

## 폴더 구조

```
web/
├── src/           # 소스 파일
│   ├── pages/     # 페이지별 HTML
│   ├── css/       # 스타일시트
│   ├── js/        # JavaScript
│   └── assets/    # 정적 리소스
├── dist/          # 빌드 산출물
├── tools/         # 빌드 도구
└── package.json
```

## 기술 스택

- Alpine.js v3 - 반응형 UI
- Tailwind CSS (CDN) - 스타일링
- DaisyUI - UI 컴포넌트
- WebSocket API - 실시간 통신
