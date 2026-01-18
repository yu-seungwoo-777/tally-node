# SPEC-WEB-001: 구현 계획

## 마일스톤

### Priority 1: 상태 데이터 파싱
- `state.js`에 `combined` 필드 초기화
- `fetchStatus()` 함수에서 `switcher.combined` 데이터 파싱 로직 추가

### Priority 2: UI 컴포넌트 구현
- `switcher.html`에 결합 Tally 카드 추가
- 듀얼 모드 조건부 표시 로직 구현

### Priority 3: 스타일링 및 테스트
- DaisyUI/TailwindCSS 클래스 적용
- 반응형 동작 확인
- 경계 케이스 테스트 (데이터 없음, 듀얼 모드 OFF)

---

## 기술 접근 방식

### 1단계: JavaScript 상태 관리 수정

**파일**: `web/src/js/modules/state.js`

**변경 사항**:
1. `status.switcher` 초기화 구조에 `combined` 필드 추가
2. `fetchStatus()` 함수에서 API 응답 처리 시 `combined` 데이터 병합

**코드 변경 위치**:
- Line 43-45: `status.switcher` 초기화
- Line 247-316: `fetchStatus()`의 switcher 데이터 처리 섹션

### 2단계: HTML 템플릿 수정

**파일**: `web/src/pages/switcher.html`

**변경 사항**:
1. Primary/Secondary 카드 섹션(Line 45-192) 뒤에 결합 Tally 카드 추가
2. 듀얼 모드 활성화(`form.switcher.dualEnabled`) 및 데이터 존재(`status.switcher.combined`) 조건부 표시

**코드 변경 위치**:
- Line 192 뒤: 새로운 Combined Tally 카드 섹션 삽입
- Line 194 이전: Camera Mapping 섹션 유지

### 3단계: Alpine.js 반응성 활용

Alpine.js의 자동 반응성 시스템을 활용하여:
- `status.switcher.combined` 객체가 변경되면 UI 자동 업데이트
- `x-show` 디렉티브로 조건부 렌더링
- `x-text` 디렉티브로 배열 데이터 문자열 변환(`join(', ')`)

---

## 데이터 흐름

```
┌─────────────────────────────────────────────────────────────────┐
│                     ESP32 Backend (C++)                         │
│  web_server_json.cpp: web_server_json_create_switcher()        │
│    - combined_tally_data[]에서 Packed 데이터 읽기                │
│    - web_server_json_create_tally()로 JSON 변환                 │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      HTTP API Response                          │
│  GET /api/status                                               │
│  {                                                             │
│    "switcher": {                                               │
│      "primary": { "tally": { "pgm": [1], "pvw": [2] } },       │
│      "secondary": { "tally": { "pgm": [3], "pvw": [4] } },     │
│      "combined": { "pgm": [1, 3], "pvw": [2, 4] },  ← 새 데이터│
│      "dualEnabled": true                                        │
│    }                                                           │
│  }                                                             │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Alpine.js State Module                       │
│  state.js: fetchStatus()                                       │
│    - data.switcher.combined 파싱                               │
│    - this.status.switcher.combined에 저장 (반응형)             │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                       UI Template (HTML)                        │
│  switcher.html: Combined Tally Card                            │
│    - x-show="dualEnabled && combined" 조건부 표시              │
│    - x-text="combined.pgm.join(', ')" PGM 리스트               │
│    - x-text="combined.pvw.join(', ')" PVW 리스트               │
└─────────────────────────────────────────────────────────────────┘
```

---

## 위험 요소 및 대응 계획

### 위험 1: API 데이터 불일치
**설명**: 듀얼 모드가 활성화되어도 `combined` 데이터가 없을 수 있음
**확률**: 낮음
**영향**: UI가 데이터를 표시하지 못함
**대응**:
- `x-show` 조건에서 `status.switcher.combined` 존재 여부 확인
- null-safe 접근 (`?.`) 사용하여 런타임 오류 방지

### 위험 2: 기존 UI 레이아웃 영향
**설명**: 새 카드 추가로 인해 스크롤 길이 증가
**확률**: 높음
**영향**: 사용자 경험 미세 변경
**대응**:
- Compact한 디자인으로 높이 최소화
- 듀얼 모드 비활성화 시 완전히 숨김으로 공간 낭비 방지

### 위험 3: 반응성 문제
**설명**: Alpine.js 반응성이 `combined` 중첩 객체에서 작동하지 않을 수 있음
**확률**: 낮음
**영향**: 데이터 변경 시 UI 업데이트 안됨
**대응**:
- `fetchStatus()`에서 새 객체 생성(`{ ...combinedData }`)으로 반응성 트리거
- 기존 Primary/Secondary 패턴과 동일한 방식 적용

---

## 구현 우선순위

1. **상태 파싱 로직**: state.js 수정 (먼저 구현하여 데이터 확인)
2. **UI 컴포넌트**: switcher.html에 카드 추가
3. **스타일링**: DaisyUI 클래스 적용 및 색상 조정
4. **테스트**: 듀얼 모드 ON/OFF 전환 시 동작 확인
