# UI 가이드 스크린샷 가이드라인

## 문서 개요

이 문서는 Tally Node Web UI의 스크린샷 캡처, 편집, 문서화를 위한 표준 가이드라인을 정의합니다.

## 스크린샷 명명 규칙

### 파일명 형식

```
{page}-{view}-{device}-{state}-{variant}.png
```

### 명명 규칙 상세

- **page**: 페이지 이름 (dashboard, network, switcher, broadcast, devices, license, system)
- **view**: 특정 뷰 또는 섹션 (main, settings, status, modal)
- **device**: 디바이스 유형 (desktop, mobile, tablet)
- **state**: UI 상태 (default, active, disabled, error, loading)
- **variant**: 변형 식별자 (01, 02, 03...)

### 예시

```
dashboard-main-desktop-default-01.png
network-settings-desktop-active-01.png
devices-status-mobile-loading-01.png
switcher-modal-desktop-default-01.png
broadcast-scan-desktop-progress-01.png
```

## 스크린샷 캡처 사양

### 해상도 요구사항

- **Desktop**: 1920x1080 (Full HD) @ 2x scale
- **Tablet**: 1024x768 @ 2x scale
- **Mobile**: 375x667 (iPhone SE) @ 2x scale

### 파일 포맷

- **포맷**: PNG (무손실 압축)
- **색상 모드**: RGB
- **투명도**: 지원 안 함 (배경 포함)

### 캡처 환경

- **브라우저**: Chrome 120+ 또는 Firefox 120+
- **확대/축소**: 100%
- **개발자 도구**: 꺼짐
- **북마크/즐겨찾기 바**: 숨김
- **다운로드 바**: 숨김

## 개인정보 마스킹 가이드

### 마스킹 대상 데이터

1. **네트워크 정보**
   - IP 주치: 192.168.1.xxx
   - MAC 주소: XX:XX:XX:XX:XX:XX
   - SSID: Tally-Node-XXXX

2. **디바이스 정보**
   - Device ID: XXXX
   - 시리얼 번호: XXXXXXXX

3. **라이선스 정보**
   - 라이선스 키: XXXX-XXXX-XXXX-XXXX

### 마스킹 스타일

- **블러 효과**: Gaussian Blur, radius 3px
- **색상覆盖**: 검정/회색 반투명 레이어
- **텍스트 대체**: XXXX 패턴

## 주석 및 화살표 가이드

### 주석 스타일

- **폰트**: SF Pro Display, Apple SD Gothic Neo, 또는 Noto Sans KR
- **크기**: 14px (desktop), 12px (mobile)
- **색상**:
  - 일반: #1e293b (slate-800)
  - 강조: #dc2626 (red-600)
  - 정보: #2563eb (blue-600)

### 화살표 스타일

- **색상**: #ef4444 (red-500)
- **두께**: 2px
- **화살표 머리**: Solid, 크기 8px
- **선 스타일**: 실선

### 번호 매기기

- **형태**: 원형 배지
- **크기**: 24x24px
- **폰트**: Bold 14px
- **배경**: #ef4444 (red-500)
- **텍스트**: 흰색

## 이미지 편집 워크플로우

### 1단계: 캡처

```bash
# Playwright 자동화 스크립트 사용
npm run screenshot:dashboard
```

### 2단계: 개인정보 마스킹

```bash
# 개인정보 마스킹 스크립트 사용
npm run privacy:mask
```

### 3단계: 주석 추가

- Figma, Photopea, 또는 GIMP 사용
- 주석 레이어 추가
- 화살표 및 번호 매기기

### 4단계: 최종 내보내기

- PNG 포맷으로 내보내기
- 최적화 (TinyPNG 또는 유사 도구)
- 문서에 통합

## 문서 통합 가이드

### Markdown 통합

```markdown
## 대시보드 페이지

### 개요
대시보드는 시스템의 전체 상태를 한눈에 확인할 수 있는 메인 페이지입니다.

### 주요 기능
- 시스템 상태 모니터링
- 배터리 및 온도 표시
- 네트워크 연결 상태
- 스위처 연결 상태

![대시보드 메인 화면](../screenshots/dashboard/dashboard-main-desktop-default-01.png)

*그림 1: 대시보드 메인 화면 (Desktop)*
```

### 이미지 경로

```
web/docs/screenshots/
├── dashboard/
│   ├── dashboard-main-desktop-default-01.png
│   ├── dashboard-status-mobile-default-01.png
│   └── ...
├── network/
│   └── ...
└── ...
```

## 품질 검증 체크리스트

### 스크린샷 품질

- [ ] 해상도가 2x 기준을 충족하는가?
- [ ] 이미지가 선명하고 흐릿하지 않은가?
- [ ] 색상이 정확하게 표현되는가?
- [ ] 파일 크기가 적절한가 (< 500KB)?

### 개인정보 보호

- [ ] 모든 IP 주소가 마스킹되었는가?
- [ ] 디바이스 ID가 마스킹되었는가?
- [ ] 라이선스 키가 마스킹되었는가?
- [ ] SSID가 마스킹되었는가?

### 주석 품질

- [ ] 주석이 명확하고 읽기 쉬운가?
- [ ] 화살표가 올바른 위치를 가리키는가?
- [ ] 번호 매기기가 논리적인 순서인가?
- [ ] 폰트 크기가 적절한가?

### 문서 통합

- [ ] 이미지가 올바른 경로에 있는가?
- [ ] Alt 텍스트가 제공되었는가?
- [ ] 캡션이 명확한가?
- [ ] 레이아웃이 일관성이 있는가?

## 도구 및 리소스

### 캡처 도구

- **Playwright**: 자동화된 스크린샷 캡처
- **Chrome DevTools**: 수동 캡처 및 디버깅

### 편집 도구

- **Figma**: 전문적인 주석 및 화살표 추가
- **Photopea**: 무료 온라인 이미지 에디터
- **GIMP**: 오픈소스 이미지 에디터

### 최적화 도구

- **TinyPNG**: 무료 이미지 압축
- **ImageOptim**: Mac용 이미지 최적화
- **Squoosh**: Google의 오픈소스 이미지 압축기

## 버전 관리

스크린샷 파일은 Git LFS (Large File Storage)를 사용하여 관리합니다.

```bash
# LFS 추가 설정
git lfs track "*.png"
git add .gitattributes
git commit -m "Add PNG files to Git LFS"
```

## 참고 문헌

- [Playwright Screenshots](https://playwright.dev/docs/screenshots)
- [Figma Design Guidelines](https://www.figma.com/best-practices/guidelines/)
- [MDN Image Optimization](https://developer.mozilla.org/en-US/docs/Learn/HTML/Multimedia_and_embedding/Responsive_images)

---

**문서 버전**: 1.0.0
**마지막 업데이트**: 2026-01-19
**유지 관리자**: MoAI-ADK Documentation Team
