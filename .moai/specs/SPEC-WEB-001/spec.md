# SPEC-WEB-001: 결합 Tally PGM/PVW 리스트 표시

## TAG BLOCK

```
SPEC-ID: SPEC-WEB-001
Title: 결합 Tally PGM/PVW 리스트 웹 UI 표시
Created: 2026-01-18
Status: Planned
Priority: Medium
Assigned: Alfred
Traceability:
  - Relates: Commit e1ec8923 (combined data API)
  - Module: web/src/js/modules/switcher.js
  - Module: web/src/pages/switcher.html
```

---

## 환경 (Environment)

### 시스템 환경
- **하드웨어**: ESP32-S3 (EoRa-S3)
- **펌웨어**: ESP-IDF 5.5.0 기반 TX 모드
- **웹 UI**: TailwindCSS + DaisyUI + Alpine.js

### 선행 조건
- 듀얼 모드가 활성화된 상태에서 Primary + Secondary Tally 데이터가 결합되어 API에 포함됨
- `/api/status` 엔드포인트가 `switcher.combined` 필드를 반환해야 함
- 결합 데이터는 이미 C++ 코드(`web_server_json.cpp`)에서 생성되어 JSON으로 제공됨

---

## 가정 (Assumptions)

1. **결합 데이터 포맷**: `combined` 객체는 `pgm`, `pvw`, `raw`, `channels` 필드를 포함함
2. **데이터 소스**: `combined_tally_data`와 `combined_channel_count`는 이미 `switcher_status_event_t`에 포함됨
3. **표시 요건**: 결합 PGM/PVW 리스트는 듀얼 모드 활성화 시에만 유효한 정보임
4. **UI 위치**: 스위처 설정 페이지의 기존 Primary/Secondary 카드 아래에 별도 섹션으로 표시

---

## 요구사항 (Requirements)

### REQ-1: 결합 Tally 데이터 수신
**WHEN** `/api/status` 엔드포인트가 호출되면, **시스템은** `switcher.combined` 필드를 포함한 전체 상태 데이터를 반환해야 한다.

**검증 방법**:
- API 응답에 `switcher.combined.pgm` 배열이 존재
- API 응답에 `switcher.combined.pvw` 배열이 존재
- API 응답에 `switcher.combined.channels` 숫자가 존재

### REQ-2: 상태 데이터 반응형 바인딩
**WHEN** API에서 `combined` 데이터가 수신되면, **시스템은** Alpine.js 반응형 상태에 해당 데이터를 저장해야 한다.

**검증 방법**:
- `status.switcher.combined` 객체가 업데이트됨
- `status.switcher.combined.pgm` 배열 접근 가능
- `status.switcher.combined.pvw` 배열 접근 가능

### REQ-3: 결합 Tally UI 표시
**WHEN** 듀얼 모드가 활성화되고 결합 데이터가 존재하면, **시스템은** 결합 PGM/PVW 채널 리스트를 스위처 페이지에 표시해야 한다.

**검증 방법**:
- 듀얼 모드 ON 시 "Combined Tally" 섹션이 표시됨
- PGM 채널들이 빨간색 박스에 쉼표로 구분되어 표시됨
- PVW 채널들이 초록색 박스에 쉼표로 구분되어 표시됨

### REQ-4: 듀얼 모드 비활성화 시 처리
**IF** 듀얼 모드가 비활성화되면, **시스템은** 결합 Tally 섹션을 표시하지 않아야 한다.

**검증 방법**:
- 듀얼 모드 OFF 시 "Combined Tally" 섹션이 숨겨짐

### REQ-5: 결합 데이터 없음 처리
**IF** 결합 데이터가 없거나 비어있으면, **시스템은** PGM/PVW 표시에 "-"를 표시해야 한다.

**검증 방법**:
- `combined`가 null 또는 undefined일 때 섹션 표시 안 함
- `pgm` 배열이 비어있을 때 "-" 표시
- `pvw` 배열이 비어있을 때 "-" 표시

---

## 상세 설명 (Specifications)

### SPEC-1: 상태 객체 확장 (state.js)

`status` 객체에 `combined` 필드를 추가하여 결합 Tally 데이터를 저장한다.

```javascript
// status.switcher 초기화 구조
status: {
    switcher: {
        dualEnabled: false,
        secondaryOffset: 4,
        primary: { ... },
        secondary: { ... },
        combined: {  // 추가됨
            pgm: [],
            pvw: [],
            raw: '',
            channels: 0
        }
    }
}
```

### SPEC-2: API 데이터 파싱 (state.js - fetchStatus)

`fetchStatus()` 함수에서 `switcher.combined` 데이터를 파싱하여 상태에 저장한다.

```javascript
// fetchStatus() 내 추가 로직
if (data.switcher && data.switcher.combined) {
    const combinedData = {
        pgm: data.switcher.combined.pgm || [],
        pvw: data.switcher.combined.pvw || [],
        raw: data.switcher.combined.raw || '',
        channels: data.switcher.combined.channels || 0
    };
    this.status.switcher.combined = combinedData;
}
```

### SPEC-3: UI 컴포넌트 추가 (switcher.html)

스위처 페이지에 결합 Tally 표시 카드를 추가한다. 위치는 Primary/Secondary 카드 아래, Camera Mapping 위에 배치한다.

**구조**:
```html
<!-- Combined Tally Card -->
<div x-show="form.switcher.dualEnabled && status.switcher.combined"
     x-transition:enter="transition ease-out duration-200"
     x-transition:enter-start="opacity-0 translate-y-2"
     x-transition:enter-end="opacity-100 translate-y-0"
     class="bg-white rounded-xl shadow-sm border border-indigo-200 p-4 mb-4">
    <div class="flex items-center justify-between mb-4">
        <div>
            <h3 class="font-semibold text-slate-900">Combined Tally (S1 + S2)</h3>
            <p class="text-xs text-slate-500 mt-0.5">결합된 프로그램/프리뷰 채널</p>
        </div>
    </div>
    <div class="flex gap-2">
        <!-- PGM -->
        <div class="flex-1 p-3 bg-red-50 rounded-lg">
            <div class="text-xs text-red-600 font-medium mb-1">PGM (Program)</div>
            <div class="text-lg font-semibold text-red-700">
                <template x-if="status.switcher.combined.pgm && status.switcher.combined.pgm.length > 0">
                    <span x-text="status.switcher.combined.pgm.join(', ')"></span>
                </template>
                <template x-if="!status.switcher.combined.pgm || status.switcher.combined.pgm.length === 0">
                    <span class="text-red-400">-</span>
                </template>
            </div>
        </div>
        <!-- PVW -->
        <div class="flex-1 p-3 bg-emerald-50 rounded-lg">
            <div class="text-xs text-emerald-600 font-medium mb-1">PVW (Preview)</div>
            <div class="text-lg font-semibold text-emerald-700">
                <template x-if="status.switcher.combined.pvw && status.switcher.combined.pvw.length > 0">
                    <span x-text="status.switcher.combined.pvw.join(', ')"></span>
                </template>
                <template x-if="!status.switcher.combined.pvw || status.switcher.combined.pvw.length === 0">
                    <span class="text-emerald-400">-</span>
                </template>
            </div>
        </div>
    </div>
</div>
```

### SPEC-4: 스타일링

결합 Tally 카드의 스타일은 다음을 따른다:
- 배경색: 흰색 (`bg-white`)
- 테두리: 인디고 (`border-indigo-200`)
- PGM 박스: 빨간색 톤 (`bg-red-50`, `text-red-700`)
- PVW 박스: 초록색 톤 (`bg-emerald-50`, `text-emerald-700`)
- 전환 효과: Alpine.js 페이드인 (`x-transition`)

---

## 추적성 (Traceability)

### 구현 파일
- `web/src/js/modules/state.js`: 상태 객체 확장, API 데이터 파싱
- `web/src/pages/switcher.html`: 결합 Tally UI 컴포넌트 추가

### 관련 커밋
- `e1ec8923`: API에 combined 데이터 추가
- `components/02_presentation/web_server/web_server_json.cpp`: combined JSON 생성 로직

### 종속 SPEC
- 없음 (독립된 UI 개선 사항)
