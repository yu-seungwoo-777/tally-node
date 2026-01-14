# SPEC-REFACTOR-002: Acceptance Criteria

## TAG BLOCK

```
TAG: SPEC-REFACTOR-002
TYPE: REFACTOR
DOMAIN: FRONTEND_UI_UX
PRIORITY: HIGH
STATUS: PLANNED
CREATED: 2025-01-15
AUTHOR: 유승우
```

---

## 1. Definition of Done

A page is considered complete when:
- [ ] All acceptance criteria for that page pass
- [ ] No console errors on load and interaction
- [ ] Mobile layout tested on at least 2 devices
- [ ] Desktop layout shows no regressions
- [ ] Touch targets meet minimum size requirements
- [ ] Code review completed

---

## 2. Navigation Acceptance Criteria

### TC-NAV-001: Bottom Navigation (Mobile)

**Given** 사용자가 모바일 화면(너비 < 768px)에서 접속할 때
**When** 메인 페이지가 로드될 때
**Then** 하단 네비게이션 바가 표시되어야 한다

**And** 하단 네비게이션은 다음 요구사항을 충족해야 한다:
- 화면 하단에 고정 위치 (bottom: 0)
- 높이는 64px (safe area 포함)
- 5개 아이템: Dashboard, Network, Devices, More(overflow)
- 활성 아이템은 강조 표시

**Given** 사용자가 하단 네비게이션 아이템을 탭할 때
**When** 탭 동작이 발생하면
**Then** 해당 페이지로 전환되어야 한다
**And** 하단 네비게이션에서 활성 상태가 업데이트되어야 한다

**Given** 사용자가 "More" 아이템을 탭할 때
**When** More 메뉴가 열리면
**Then** 다음 항목을 포함하는 오버레이 메뉴가 표시되어야 한다:
- Switcher
- Broadcast
- License
- System

---

### TC-NAV-002: Sidebar Navigation (Desktop)

**Given** 사용자가 데스크톱 화면(너비 >= 768px)에서 접속할 때
**When** 메인 페이지가 로드될 때
**Then** 사이드바 네비게이션이 표시되어야 한다
**And** 하단 네비게이션은 숨겨져야 한다

**Given** 사용자가 사이드바 메뉴 아이템을 클릭할 때
**When** 클릭 동작이 발생하면
**Then** 해당 페이지로 전환되어야 한다
**And** 사이드바에서 활성 상태가 업데이트되어야 한다

---

### TC-NAV-003: Responsive Breakpoint Switch

**Given** 사용자가 모바일 화면에서 앱을 사용할 때
**When** 화면 너비가 768px 이상으로 확장되면
**Then** 하단 네비게이션이 사이드바로 전환되어야 한다

**Given** 사용자가 데스크톱 화면에서 앱을 사용할 때
**When** 화면 너비가 768px 미만으로 축소되면
**Then** 사이드바가 하단 네비게이션으로 전환되어야 한다

---

## 3. Dashboard Page Acceptance Criteria

### TC-DASH-001: Stats Cards (Mobile)

**Given** 사용자가 모바일 화면에서 Dashboard 페이지를 볼 때
**When** 페이지가 로드되면
**Then** 배터리, 온도, 업타임 카드가 다음 요구사항을 충족해야 한다:
- 가로 스크롤 스냅 컨테이너로 표시
- 한 화면에 하나의 카드가 완전히 표시
- 스와이프로 다음 카드로 이동 가능

---

### TC-DASH-002: Switcher/Network Cards (Mobile)

**Given** 사용자가 모바일 화면에서 Dashboard 페이지를 볼 때
**When** Switcher Status 또는 Network Status 카드를 볼 때
**Then** 카드는 기본적으로 축소된 상태여야 한다
**And** 카드 헤더를 탭하면 확장/축소되어야 한다

---

### TC-DASH-003: System Info (Mobile)

**Given** 사용자가 모바일 화면에서 Dashboard 페이지를 볼 때
**When** System Info 섹션이 로드되면
**Then** 기본적으로 접힌 상태여야 한다
**And** "Show More" 버튼을 탭하면 펼쳐져야 한다

---

## 4. Devices Page Acceptance Criteria

### TC-DEV-001: Device Card Layout (Mobile)

**Given** 사용자가 모바일 화면에서 Devices 페이지를 볼 때
**When** 디바이스 카드가 표시되면
**Then** 다음 요구사항을 충족해야 한다:
- 단일 열 레이아웃 (전체 너비)
- 카드 간 간격: 16px
- 상태 정보가 명확하게 표시

---

### TC-DEV-002: Swipe Gestures

**Given** 사용자가 모바일 화면에서 온라인 디바이스 카드를 볼 때
**When** 카드를 왼쪽으로 스와이프하면
**Then** 빠른 액션 메뉴가 표시되어야 한다:
- Stop/Resume 버튼
- 밝기 프리셋 버튼

**Given** 사용자가 모바일 화면에서 디바이스 카드를 볼 때
**When** 카드를 오른쪽으로 스와이프하면
**Then** 파괴적 액션 메뉴가 표시되어야 한다:
- Reboot 버튼 (주황색)
- Delete 버튼 (빨간색)

**Given** 스와이프 메뉴가 열려 있을 때
**When** 카드 외부를 탭하면
**Then** 메뉴가 닫혀야 한다

---

### TC-DEV-003: Device Card Controls (Mobile)

**Given** 사용자가 모바일 화면에서 온라인 디바이스 카드를 볼 때
**When** 밝기 슬라이더를 조정하면
**Then** 슬라이더는 다음 요구사항을 충족해야 한다:
- 높이: 최소 44px (터치 타겟)
- 부드러운 드래그 동작
- 실시간 값 표시

**Given** 사용자가 모바일 화면에서 온라인 디바이스 카드를 볼 때
**When** Camera ID 조정 버튼을 탭하면
**Then** 카메라 ID가 1씩 증가/감소해야 한다
**And** 변경 사항이 즉시 반영되어야 한다

---

## 5. Network Page Acceptance Criteria

### TC-NET-001: Accordion Pattern (Mobile)

**Given** 사용자가 모바일 화면에서 Network 페이지를 볼 때
**When** 페이지가 로드되면
**Then** 네트워크 설정 섹션은 다음 요구사항을 충족해야 한다:
- AP, WiFi, Ethernet 설정이 각각 독립적인 아코디언
- 기본적으로 첫 번째 섹션만 펼쳐진 상태
- 한 번에 하나의 섹션만 펼쳐질 수 있음

---

### TC-NET-002: Form Inputs (Mobile)

**Given** 사용자가 모바일 화면에서 WiFi 설정을 수정할 때
**When** SSID 또는 Password 입력 필드를 탭하면
**Then** 다음 요구사항을 충족해야 한다:
- 입력 필드 높이: 최소 44px
- 키보드가 입력 필드를 가리지 않음
- 자동 포커스 및 스크롤

---

### TC-NET-003: Status Display (Mobile)

**Given** 사용자가 모바일 화면에서 Network 페이지를 볼 때
**When** 상태 카드가 표시되면
**Then** AP, WiFi, Ethernet 상태가 축약된 형태로 표시되어야 한다
**And** 각 상태는 다음 정보를 포함해야 한다:
- 연결 상태 (Connected/Disconnected)
- SSID 또는 IP 주소

---

## 6. Switcher Page Acceptance Criteria

### TC-SWT-001: Dual Mode Toggle (Mobile)

**Given** 사용자가 모바일 화면에서 Switcher 페이지를 볼 때
**When** 페이지를 스크롤하면
**Then** Dual Mode 토글이 상단에 고정(sticky)되어야 한다
**And** 항상 접근 가능해야 한다

---

### TC-SWT-002: Camera Mapping (Mobile)

**Given** 사용자가 모바일 화면에서 Switcher 페이지를 볼 때
**When** Camera Mapping 그리드가 표시되면
**Then** 다음 요구사항을 충족해야 한다:
- 가로 스크롤 가능
- 한 화면에 5개 카메라 버튼 표시
- 스와이프로 더 많은 카메라 접근

---

### TC-SWT-003: Tally Status (Mobile)

**Given** 사용자가 모바일 화면에서 Switcher 페이지를 볼 때
**When** Switcher 상태 카드가 표시되면
**Then** PGM/PVW 상태는 다음 요구사항을 충족해야 한다:
- 컴팩트 뱃지 형태
- 숫자 목록으로 표시 (예: "1, 3, 5")
- 색상 구분 (PGM: 빨간색, PVW: 초록색)

---

## 7. Broadcast Page Acceptance Criteria

### TC-BCAST-001: Scan Results (Mobile)

**Given** 사용자가 모바일 화면에서 Broadcast 페이지를 볼 때
**When** 채널 스캔 결과가 표시되면
**Then** 다음 요구사항을 충족해야 한다:
- 세로 목록 형태
- 각 결과는 최소 48px 높이
- 터치하여 채널 선택 가능

---

### TC-BCAST-002: Scan Button (Mobile)

**Given** 사용자가 모바일 화면에서 Broadcast 페이지를 볼 때
**When** 페이지가 로드되면
**Then** "Start Scan" 버튼이 하단에 고정되어야 한다
**And** 스크롤 중에도 항상 표시되어야 한다

---

### TC-BCAST-003: Frequency Slider (Mobile)

**Given** 사용자가 모바일 화면에서 Broadcast 페이지를 볼 때
**When** 주파수 슬라이더를 조정하면
**Then** 다음 요구사항을 충족해야 한다:
- 슬라이더 높이: 최소 44px
- 드래그 핸들이 크게 표시
- 실시간 주파수 값 표시

---

## 8. License Page Acceptance Criteria

### TC-LIC-001: License Input (Mobile)

**Given** 사용자가 모바일 화면에서 License 페이지를 볼 때
**When** 라이선스 키 입력 필드를 탭하면
**Then** 다음 요구사항을 충족해야 한다:
- 입력 필드가 전체 너비
- 높이: 최소 48px
- 대문자 자동 변환
- 자동 포맷팅 (XXXX-XXXX-XXXX-XXXX)

---

### TC-LIC-002: Test Results (Mobile)

**Given** 사용자가 모바일 화면에서 License 페이지를 볼 때
**When** 인터넷 연결 테스트를 실행하면
**Then** 결과는 다음 요구사항을 충족해야 한다:
- 컴팩트한 상태 표시
- 성공/실패를 색상으로 구분
- 핑 값이 명확하게 표시

---

## 9. System Page Acceptance Criteria

### TC-SYS-001: System Actions (Mobile)

**Given** 사용자가 모바일 화면에서 System 페이지를 볼 때
**When** System Actions 섹션이 표시되면
**Then** 다음 요구사항을 충족해야 한다:
- "Reboot TX"와 "Reboot All" 버튼이 크게 표시
- 각 버튼 높이: 최소 48px
- 경고 색상 사용 (주황색/빨간색)

---

### TC-SYS-002: Test Mode Controls (Mobile)

**Given** 사용자가 모바일 화면에서 System 페이지를 볼 때
**When** Test Mode 설정을 조정하면
**Then** 다음 요구사항을 충족해야 한다:
- Max Channels 입력: 넓은 터치 타겟
- Interval 슬라이더: 높이 48px 이상
- Start/Stop 버튼: 명확한 상태 표시

---

### TC-SYS-003: Update Speed (Mobile)

**Given** 사용자가 모바일 화면에서 System 페이지를 볼 때
**When** Update Speed 버튼을 탭하면
**Then** 다음 요구사항을 충족해야 한다:
- 4개 버튼이 2x2 그리드로 표시
- 각 버튼의 최소 높이: 60px
- 선택된 버튼이 명확하게 표시

---

## 10. Accessibility Acceptance Criteria

### TC-ACC-001: Touch Targets

**Given** 사용자가 모바일 화면에서 어떤 페이지를 볼 때
**When** 모든 대화형 요소를 검사할 때
**Then** 다음 요구사항을 충족해야 한다:
- 버튼: 최소 44x44px
- 링크: 최소 44px 높이
- 체크박스: 최소 44x44px
- 라디오: 최소 44x44px
- 슬라이더: 트랙 높이 최소 44px

---

### TC-ACC-002: Color Contrast

**Given** 사용자가 어떤 페이지를 볼 때
**When** 모든 텍스트 요소를 검사할 때
**Then** 다음 요구사항을 충족해야 한다:
- 일반 텍스트: WCAG AA (4.5:1)
- 큰 텍스트 (18px+): WCAG AA (3:1)
- UI 컴포넌트: WCAG AA (3:1)

---

### TC-ACC-003: No Horizontal Scroll

**Given** 사용자가 모바일 화면에서 어떤 페이지를 볼 때
**When** 페이지를 세로로 스크롤할 때
**Then** 가로 스크롤이 발생해서는 안 된다
**And** 모든 콘텐츠가 화면 너비 내에 맞아야 한다

---

### TC-ACC-004: Focus Management

**Given** 사용자가 키보드로 페이지를 탐색할 때
**When** Tab 키를 누르면
**Then** 포커스가 논리적인 순서로 이동해야 한다
**And** 포커스된 요소가 명확하게 표시되어야 한다

---

### TC-ACC-005: Screen Reader Support

**Given** 사용자가 스크린 리더를 사용할 때
**When** 페이지가 로드되면
**Then** 다음 요구사항을 충족해야 한다:
- 모든 버튼에 접근 가능한 레이블
- 상태 변경이 aria-live로 발표됨
- 모달이 aria-modal로 표시됨
- 네비게이션에 ARIA 레이블

---

## 11. Performance Acceptance Criteria

### TC-PERF-001: First Contentful Paint

**Given** 사용자가 어떤 페이지에 접속할 때
**When** 페이지 로드가 시작되면
**Then** 첫 번째 콘텐츠 페인트(FCP)가 1.5초 이내에 발생해야 한다

---

### TC-PERF-002: Interaction Latency

**Given** 사용자가 대화형 요소를 탭할 때
**When** 탭 동작이 발생하면
**Then** 시각적 피드백이 100ms 이내에 표시되어야 한다

---

### TC-PERF-003: Layout Stability

**Given** 사용자가 페이지를 탐색할 때
**When** 페이지 전환이 발생하면
**Then** 누적 레이아웃 변경(CLS)이 0.1 미만이어야 한다

---

## 12. Regression Prevention Criteria

### TC-REG-001: Desktop Experience

**Given** 사용자가 데스크톱 화면에서 앱을 사용할 때
**When** 모든 페이지를 탐색할 때
**Then** 기존 기능에 영향이 없어야 한다:
- 사이드바 네비게이션 정상 작동
- 모든 설정 변경 가능
- WebSocket 연결 안정적
- 모든 API 호출 정상 작동

---

### TC-REG-002: API Contract

**Given** 사용자가 어떤 페이지에서든 액션을 수행할 때
**When** API 요청이 발생하면
**Then** API 계약이 변경되지 않아야 한다:
- 요청 형식 동일
- 응답 형식 동일
- WebSocket 메시지 형식 동일

---

### TC-REG-003: Alpine.js State

**Given** 사용자가 페이지 간을 전환할 때
**When** 전환이 발생하면
**Then** Alpine.js 상태 관리가 정상 작동해야 한다:
- currentView 상태 업데이트
- 폼 데이터 유지
- WebSocket 연결 유지

---

## 13. Cross-Browser Acceptance Criteria

### TC-BRW-001: iOS Safari

**Given** 사용자가 iPhone (iOS 16+)에서 Safari로 접속할 때
**When** 모든 페이지를 사용할 때
**Then** 모든 기능이 정상 작동해야 한다
**And** notched 디바이스에서 safe area가 처리되어야 한다

---

### TC-BRW-002: Android Chrome

**Given** 사용자가 Android (12+)에서 Chrome으로 접속할 때
**When** 모든 페이지를 사용할 때
**Then** 모든 기능이 정상 작동해야 한다
**And**Material Design 가이드라인과 호환되어야 한다

---

## 14. Quality Gates

### Gate 1: Milestone 1 Complete
- [ ] TC-NAV-001, TC-NAV-002, TC-NAV-003 통과
- [ ] iOS Safari 및 Android Chrome 기본 기능 작동 확인

### Gate 2: Milestone 2 Complete
- [ ] TC-DASH-001~003 통과
- [ ] TC-DEV-001~003 통과
- [ ] 모바일에서 주요 사용자 경험 확인

### Gate 3: Milestone 3 Complete
- [ ] TC-NET-001~003 통과
- [ ] TC-SWT-001~003 통과

### Gate 4: Milestone 4 Complete
- [ ] TC-BCAST-001~003 통과
- [ ] TC-LIC-001~002 통과
- [ ] TC-SYS-001~003 통과

### Gate 5: Milestone 5 Complete
- [ ] 모든 페이지에서 일관된 디자인 적용
- [ ] 색상/간격/타이포그래피 통일

### Gate 6: Milestone 6 Complete
- [ ] TC-ACC-001~005 통과
- [ ] TC-PERF-001~003 통과
- [ ] TC-REG-001~003 통과
- [ ] TC-BRW-001~002 통과

---

## 15. Final Acceptance

**Release Criteria:**
- [ ] 모든 Quality Gates 통과
- [ ] 실제 기기에서 QA 팀 승인
- [ ] 성능 벤치마크 목표 달성
- [ ] 접근성 감사 통과
- [ ] 데스크톱 회귀 없음 확인
