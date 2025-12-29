# Switcher Settings 섹션 디자인 명세

## 개요

듀얼 스위처 지원을 위한 Switcher Settings 섹션의 UI/UX 디자인 명세서입니다.

---

## 1. 레이아웃 구조

```
┌─────────────────────────────────────────────────────────────────┐
│ Switcher Settings                                               │
│ Configure video switcher connections                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ ┌───────────────────────────────────────────────────────────┐  │
│ │  Dual Mode                                    [Toggle]     │  │
│ │  Enable secondary switcher for expanded camera coverage   │  │
│ └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│ ┌────────────────────────────┬────────────────────────────────┐│
│ │ Primary (S1)               │ Secondary (S2)                 ││
│ │ ────────────────────────   │ ────────────────────────────   ││
│ │ [Connected ✓]              │ [Disconnected ✗]              ││
│ │                            │                                ││
│ │ Type: ATEM                 │ Type: ATEM                     ││
│ │ IP:   192.168.0.240        │ IP:   192.168.0.244            ││
│ │ Port: 9910                 │ Port: 9910                     ││
│ │                            │                                ││
│ │ ┌────────────────────────┐ │ ┌────────────────────────┐     ││
│ │ │ PGM: [4]              │ │ │ PGM: [-]               │     ││
│ │ │ PVW: [1]              │ │ │ PVW: [-]               │     ││
│ │ └────────────────────────┘ │ └────────────────────────┘     ││
│ │                            │                                ││
│ │ [Configure]                │ [Configure]                   ││
│ └────────────────────────────┴────────────────────────────────┘│
│                                                                 │
│ ┌───────────────────────────────────────────────────────────┐  │
│ │  Camera Mapping                                            │  │
│ │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐   │  │
│ │  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │ 9  │ 10 │   │  │
│ │  ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┤   │  │
│ │  │ 11 │ 12 │ 13 │ 14 │ 15 │ 16 │ 17 │ 18 │ 19 │ 20 │   │  │
│ │  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘   │  │
│ │                                                             │  │
│ │  🔵 Primary     🟣 Secondary    [Secondary starts at: 5]   │  │
│ │                                                             │  │
│ │  Click a camera button to set where Secondary takes over    │  │
│ │  [Save Mapping]                                             │  │
│ └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│ ┌────────────────────────────┬────────────────────────────────┐│
│ │ Primary Configuration       │ Secondary Configuration        ││
│ │ (Dialog or expand)          │ (Dialog or expand)             ││
│ └────────────────────────────┴────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 컴포넌트별 디자인

### 2.1 Dual Mode 토글

| 속성 | 값 |
|------|-----|
| 위치 | 섹션 상단 |
| 스타일 | Toggle Switch |
| 배경 | `bg-white`, `border-slate-200` |
| 활성화 색상 | `bg-blue-600` |

**동작:**
- Toggle ON → Secondary 카드 활성화 + Camera Mapping 섹션 표시
- Toggle OFF → Secondary 카드 비활성화 (opacity 60%) + Camera Mapping 숨김

### 2.2 스위처 상태 카드 (2열 배치)

각 스위처별 상태를 표시하는 카드입니다.

| 속성 | Primary | Secondary |
|------|---------|-----------|
| 헤더 배경 | `bg-blue-50` | `bg-purple-50` (활성시) |
| 보더 색상 | `border-blue-200` | `border-purple-200` (활성시) |
| 인디케이터 | `bg-blue-600` | `bg-purple-600` |
| PGM 뱃지 | `bg-red-100 text-red-700` | 동일 |
| PVW 뱃지 | `bg-green-100 text-green-700` | 동일 |

**연결 상태 뱃지:**
- Connected: `bg-emerald-100 text-emerald-700`
- Disconnected: `bg-rose-100 text-rose-700`

**Tally 상태 표시:**
- PGM (Program): 빨간색 뱃지 + 채널 번호 목록
- PVW (Preview): 초록색 뱃지 + 채널 번호 목록
- 비어있을 때: `-` 표시

#### API 데이터 구조
```json
{
  "switcher": {
    "primary": {
      "connected": true,
      "type": "ATEM",
      "ip": "192.168.0.240",
      "port": 9910,
      "interface": 2,
      "cameraLimit": 0,
      "tally": {
        "pgm": [4],
        "pvw": [1],
        "raw": "42",
        "channels": 4
      }
    },
    "secondary": {
      "connected": false,
      "type": "ATEM",
      "ip": "192.168.0.244",
      "port": 9910,
      "interface": 1,
      "cameraLimit": 0,
      "tally": {
        "pgm": [],
        "pvw": [],
        "raw": "0000000000",
        "channels": 20
      }
    },
    "dualEnabled": true,
    "secondaryOffset": 4
  }
}
```

### 2.3 스위처 설정 (Dialog/Expand)

상태 카드의 `[Configure]` 버튼 클릭 시 표시되는 설정 폼입니다.

| 항목 | Primary | Secondary |
|------|---------|-----------|
| Type | `select` (ATEM/vMix/OBS) | 동일 |
| IP Address | `input` (text) | 동일 |
| Port | `input` (number) | 동일 |
| 저장 버튼 | `bg-blue-600` | `bg-purple-600` |

### 2.4 Camera Mapping 시각화

#### 버튼 그리드
- 20개 버튼 (10열 × 2행)
- 버튼 크기: `aspect-square` (정사각형)
- 간격: `gap-2`

#### 버튼 상태별 스타일

| 상태 | 색상 | Tailwind Class | 설명 |
|------|------|----------------|------|
| Primary | Blue | `bg-blue-500 text-white` | 카메라 1 ~ (offset-1) |
| Secondary | Purple | `bg-purple-500 text-white` | 카메라 offset ~ 20 |
| 선택된 Offset | Dark Purple | `bg-purple-700 ring-2 ring-purple-400 ring-offset-2` | Secondary 시작 위치 |
| 비활성화 | Gray | `bg-slate-100 text-slate-400 cursor-not-allowed` | Dual Mode OFF 시 |

#### Hover 효과
- Primary: `hover:bg-blue-600`
- Secondary: `hover:bg-purple-600`
- 선택된 Offset: `hover:bg-purple-800`

---

## 3. 컬러 스킴

### Primary 스위처 (S1)
```
Indicator:  bg-blue-600   (#2563eb)
Card Border:border-blue-200 (#bfdbfe)
Header:     bg-blue-50    (#eff6ff)
Button:     bg-blue-500   (#3b82f6)
Button Hover:bg-blue-600   (#2563eb)
```

### Secondary 스위처 (S2)
```
Indicator:  bg-purple-600 (#9333ea)
Card Border:border-purple-200 (#e9d5ff)
Header:     bg-purple-50  (#faf5ff)
Button:     bg-purple-500 (#a855f7)
Button Hover:bg-purple-600 (#9333ea)
Selected:   bg-purple-700 (#7e22ce)
```

---

## 4. 데이터 구조 (Alpine.js)

### 4.1 form.switcher 확장

```javascript
form: {
    switcher: {
        primary: {
            type: 'ATEM',      // ATEM | vMix | OBS
            ip: '',            // xxx.xxx.xxx.xxx
            port: 9910         // 포트 번호
        },
        secondary: {
            type: 'ATEM',
            ip: '',
            port: 9910
        },
        dualEnabled: false,    // 듀얼 모드 활성화
        secondaryOffset: 4     // 1-based (1~20)
    },
    mappingOffset: 4           // 매핑 UI용 임시 offset
}
```

### 4.2 config.switcher (기존)

```javascript
config: {
    switcher: {
        primary: {
            connected: false,
            type: 'ATEM',
            ip: '',
            port: 0,
            interface: 2,
            cameraLimit: 0
        },
        secondary: {
            connected: false,
            type: 'ATEM',
            ip: '',
            port: 0,
            interface: 1,
            cameraLimit: 0
        },
        dualEnabled: false,
        secondaryOffset: 4      // 0-based (0~19)
    }
}
```

---

## 5. JavaScript 함수

### 5.1 카메라 버튼 스타일 계산

```javascript
getCameraButtonClass(cameraNum) {
    if (!this.form.switcher.dualEnabled) {
        return 'bg-slate-100 text-slate-400 cursor-not-allowed';
    }

    const offset = this.form.mappingOffset;

    // Secondary 시작 위치 (선택된 offset)
    if (cameraNum === offset) {
        return 'bg-purple-700 text-white ring-2 ring-purple-400 ring-offset-2 cursor-pointer hover:bg-purple-800';
    }
    // Secondary 영역
    if (cameraNum > offset) {
        return 'bg-purple-500 text-white cursor-pointer hover:bg-purple-600';
    }
    // Primary 영역
    return 'bg-blue-500 text-white cursor-pointer hover:bg-blue-600';
}
```

### 5.2 Offset 선택

```javascript
selectOffset(cameraNum) {
    if (!this.form.switcher.dualEnabled) return;
    this.form.mappingOffset = cameraNum;
}
```

### 5.3 듀얼 모드 변경

```javascript
onDualModeChange() {
    if (this.form.switcher.dualEnabled) {
        this.form.mappingOffset = this.config.switcher.secondaryOffset + 1; // 1-based 변환
    }
}
```

---

## 6. API 설계

### 6.1 기존 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/api/config/switcher/primary` | POST | Primary 설정 저장 |
| `/api/config/switcher/secondary` | POST | Secondary 설정 저장 |

### 6.2 신규 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/api/config/switcher/mapping` | POST | 듀얼 모드 + Offset 저장 |

#### `/api/config/switcher/mapping` Request

```json
{
    "dualEnabled": true,
    "secondaryOffset": 3   // 0-based (0~19)
}
```

#### Response

```json
{
    "status": "ok"
}
```

---

## 7. HTML 구조

```html
<!-- Switcher Section -->
<div x-show="currentView === 'switcher'" x-cloak>
    <!-- Dual Mode Toggle -->
    <div class="bg-white rounded-xl shadow-sm border border-slate-200 p-4 mb-4">
        <div class="flex items-center justify-between">
            <div>
                <h3 class="font-semibold text-slate-900">Dual Mode</h3>
                <p class="text-sm text-slate-500">Enable secondary switcher for expanded camera coverage</p>
            </div>
            <label class="relative inline-flex items-center cursor-pointer">
                <input type="checkbox" x-model="form.switcher.dualEnabled"
                       class="sr-only peer" @change="onDualModeChange()">
                <div class="w-11 h-6 bg-slate-200 rounded-full peer
                            peer-checked:after:translate-x-full
                            after:absolute after:top-[2px] after:left-[2px]
                            after:bg-white after:rounded-full after:h-5 after:w-5
                            peer-checked:bg-blue-600 transition-all"></div>
            </label>
        </div>
    </div>

    <!-- Switcher Status Cards (2-Column) -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
        <!-- Primary Status Card -->
        <div class="bg-white rounded-xl shadow-sm border border-blue-200 overflow-hidden">
            <div class="px-4 py-3 bg-blue-50 border-b border-blue-100">
                <div class="flex items-center justify-between">
                    <div class="flex items-center gap-2">
                        <span class="w-3 h-3 rounded-full bg-blue-600"></span>
                        <h3 class="font-semibold text-slate-900">Primary (S1)</h3>
                    </div>
                    <span class="text-xs px-2 py-1 rounded-full"
                          :class="status.switcher.primary.connected
                              ? 'bg-emerald-100 text-emerald-700'
                              : 'bg-rose-100 text-rose-700'">
                        <span x-text="status.switcher.primary.connected ? 'Connected' : 'Disconnected'"></span>
                    </span>
                </div>
            </div>
            <div class="p-4 space-y-3">
                <div class="text-sm text-slate-600">
                    <span class="font-medium">Type:</span>
                    <span x-text="status.switcher.primary.type || '--'"></span>
                </div>
                <div class="text-sm text-slate-600">
                    <span class="font-medium">IP:</span>
                    <span class="font-mono" x-text="status.switcher.primary.ip || '--'"></span>
                </div>
                <div class="text-sm text-slate-600">
                    <span class="font-medium">Port:</span>
                    <span x-text="status.switcher.primary.port || '--'"></span>
                </div>

                <!-- Tally Status -->
                <div class="flex gap-2">
                    <div class="flex-1 p-2 bg-red-50 rounded-lg">
                        <div class="text-xs text-red-600 font-medium mb-1">PGM</div>
                        <div class="text-sm font-semibold text-red-700">
                            <template x-if="status.switcher.primary.tally.pgm.length > 0">
                                <span x-text="status.switcher.primary.tally.pgm.join(', ')"></span>
                            </template>
                            <template x-if="status.switcher.primary.tally.pgm.length === 0">
                                <span class="text-red-400">-</span>
                            </template>
                        </div>
                    </div>
                    <div class="flex-1 p-2 bg-emerald-50 rounded-lg">
                        <div class="text-xs text-emerald-600 font-medium mb-1">PVW</div>
                        <div class="text-sm font-semibold text-emerald-700">
                            <template x-if="status.switcher.primary.tally.pvw.length > 0">
                                <span x-text="status.switcher.primary.tally.pvw.join(', ')"></span>
                            </template>
                            <template x-if="status.switcher.primary.tally.pvw.length === 0">
                                <span class="text-emerald-400">-</span>
                            </template>
                        </div>
                    </div>
                </div>

                <button @click="showPrimaryConfig = true"
                        class="w-full py-2 text-sm font-medium text-blue-600
                               border border-blue-200 rounded-lg hover:bg-blue-50">
                    Configure
                </button>
            </div>
        </div>

        <!-- Secondary Status Card -->
        <div class="bg-white rounded-xl shadow-sm border overflow-hidden"
             :class="form.switcher.dualEnabled
                 ? 'border-purple-200'
                 : 'border-slate-200 opacity-60'">
            <div class="px-4 py-3 border-b"
                 :class="form.switcher.dualEnabled
                     ? 'bg-purple-50 border-purple-100'
                     : 'bg-slate-50 border-slate-100'">
                <div class="flex items-center justify-between">
                    <div class="flex items-center gap-2">
                        <span class="w-3 h-3 rounded-full"
                              :class="form.switcher.dualEnabled ? 'bg-purple-600' : 'bg-slate-400'"></span>
                        <h3 class="font-semibold text-slate-900">Secondary (S2)</h3>
                    </div>
                    <span class="text-xs px-2 py-1 rounded-full"
                          :class="status.switcher.secondary.connected
                              ? 'bg-emerald-100 text-emerald-700'
                              : 'bg-rose-100 text-rose-700'">
                        <span x-text="status.switcher.secondary.connected ? 'Connected' : 'Disconnected'"></span>
                    </span>
                </div>
            </div>
            <div class="p-4 space-y-3">
                <div class="text-sm text-slate-600">
                    <span class="font-medium">Type:</span>
                    <span x-text="status.switcher.secondary.type || '--'"></span>
                </div>
                <div class="text-sm text-slate-600">
                    <span class="font-medium">IP:</span>
                    <span class="font-mono" x-text="status.switcher.secondary.ip || '--'"></span>
                </div>
                <div class="text-sm text-slate-600">
                    <span class="font-medium">Port:</span>
                    <span x-text="status.switcher.secondary.port || '--'"></span>
                </div>

                <!-- Tally Status -->
                <div class="flex gap-2">
                    <div class="flex-1 p-2 bg-red-50 rounded-lg">
                        <div class="text-xs text-red-600 font-medium mb-1">PGM</div>
                        <div class="text-sm font-semibold text-red-700">
                            <template x-if="status.switcher.secondary.tally.pgm.length > 0">
                                <span x-text="status.switcher.secondary.tally.pgm.join(', ')"></span>
                            </template>
                            <template x-if="status.switcher.secondary.tally.pgm.length === 0">
                                <span class="text-red-400">-</span>
                            </template>
                        </div>
                    </div>
                    <div class="flex-1 p-2 bg-emerald-50 rounded-lg">
                        <div class="text-xs text-emerald-600 font-medium mb-1">PVW</div>
                        <div class="text-sm font-semibold text-emerald-700">
                            <template x-if="status.switcher.secondary.tally.pvw.length > 0">
                                <span x-text="status.switcher.secondary.tally.pvw.join(', ')"></span>
                            </template>
                            <template x-if="status.switcher.secondary.tally.pvw.length === 0">
                                <span class="text-emerald-400">-</span>
                            </template>
                        </div>
                    </div>
                </div>

                <button @click="form.switcher.dualEnabled && (showSecondaryConfig = true)"
                        class="w-full py-2 text-sm font-medium rounded-lg"
                        :class="form.switcher.dualEnabled
                            ? 'text-purple-600 border border-purple-200 hover:bg-purple-50'
                            : 'text-slate-400 border border-slate-200 cursor-not-allowed'"
                        :disabled="!form.switcher.dualEnabled">
                    Configure
                </button>
            </div>
        </div>
    </div>

    <!-- Camera Mapping Visualization -->
    <div class="bg-white rounded-xl shadow-sm border border-slate-200 p-4 mb-4"
         x-show="form.switcher.dualEnabled"
         x-transition:enter="transition ease-out duration-200"
         x-transition:enter-start="opacity-0 translate-y-2"
         x-transition:enter-end="opacity-100 translate-y-0">
        <div class="flex items-center justify-between mb-4">
            <h3 class="font-semibold text-slate-900">Camera Mapping</h3>
            <div class="flex items-center gap-4 text-sm">
                <div class="flex items-center gap-1.5">
                    <span class="w-3 h-3 rounded bg-blue-500"></span>
                    <span class="text-slate-600">Primary</span>
                </div>
                <div class="flex items-center gap-1.5">
                    <span class="w-3 h-3 rounded bg-purple-500"></span>
                    <span class="text-slate-600">Secondary</span>
                </div>
            </div>
        </div>

        <!-- Camera Buttons Grid -->
        <div class="grid grid-cols-10 gap-2 mb-4">
            <template x-for="i in 20" :key="i">
                <button @click="selectOffset(i)"
                        class="aspect-square rounded-lg font-medium text-sm transition-all"
                        :class="getCameraButtonClass(i)"
                        :disabled="!form.switcher.dualEnabled"
                        x-text="i">
                </button>
            </template>
        </div>

        <!-- Offset Info & Save -->
        <div class="flex items-center justify-between p-3 bg-slate-50 rounded-lg">
            <div class="text-sm text-slate-600">
                Secondary starts at camera
                <span class="font-bold text-purple-600" x-text="form.mappingOffset"></span>
            </div>
            <button @click="saveMapping()"
                    class="px-4 py-2 text-sm font-medium text-white
                           bg-purple-600 rounded-lg hover:bg-purple-700">
                Save Mapping
            </button>
        </div>
    </div>
</div>

<!-- Primary Configuration Dialog -->
<div x-show="showPrimaryConfig" x-cloak
     class="fixed inset-0 z-50 flex items-center justify-center bg-black/50"
     x-transition:enter="transition ease-out duration-200"
     x-transition:enter-start="opacity-0"
     x-transition:enter-end="opacity-100">
    <div class="bg-white rounded-xl shadow-xl w-full max-w-md mx-4"
         x-transition:enter="transition ease-out duration-200"
         x-transition:enter-start="opacity-0 scale-95"
         x-transition:enter-end="opacity-100 scale-100">
        <div class="px-4 py-3 bg-blue-50 border-b border-blue-100 flex items-center justify-between">
            <h3 class="font-semibold text-slate-900">Primary Switcher Configuration</h3>
            <button @click="showPrimaryConfig = false" class="text-slate-400 hover:text-slate-600">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
                </svg>
            </button>
        </div>
        <div class="p-4 space-y-4">
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">Type</label>
                <select x-model="form.switcher.primary.type"
                        class="w-full px-3 py-2 border border-slate-300 rounded-lg">
                    <option value="ATEM">ATEM</option>
                    <option value="vMix">vMix</option>
                    <option value="OBS">OBS</option>
                </select>
            </div>
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">IP Address</label>
                <input type="text" x-model="form.switcher.primary.ip"
                       class="w-full px-3 py-2 border border-slate-300 rounded-lg"
                       placeholder="192.168.1.100">
            </div>
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">Port</label>
                <input type="number" x-model="form.switcher.primary.port"
                       class="w-full px-3 py-2 border border-slate-300 rounded-lg">
            </div>
            <div class="flex gap-2">
                <button @click="showPrimaryConfig = false"
                        class="flex-1 py-2 text-sm font-medium text-slate-600
                               border border-slate-300 rounded-lg hover:bg-slate-50">
                    Cancel
                </button>
                <button @click="savePrimarySwitcher()"
                        class="flex-1 py-2 text-sm font-medium text-white
                               bg-blue-600 rounded-lg hover:bg-blue-700">
                    Save
                </button>
            </div>
        </div>
    </div>
</div>

<!-- Secondary Configuration Dialog -->
<div x-show="showSecondaryConfig" x-cloak
     class="fixed inset-0 z-50 flex items-center justify-center bg-black/50"
     x-transition:enter="transition ease-out duration-200"
     x-transition:enter-start="opacity-0"
     x-transition:enter-end="opacity-100">
    <div class="bg-white rounded-xl shadow-xl w-full max-w-md mx-4"
         x-transition:enter="transition ease-out duration-200"
         x-transition:enter-start="opacity-0 scale-95"
         x-transition:enter-end="opacity-100 scale-100">
        <div class="px-4 py-3 bg-purple-50 border-b border-purple-100 flex items-center justify-between">
            <h3 class="font-semibold text-slate-900">Secondary Switcher Configuration</h3>
            <button @click="showSecondaryConfig = false" class="text-slate-400 hover:text-slate-600">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
                </svg>
            </button>
        </div>
        <div class="p-4 space-y-4">
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">Type</label>
                <select x-model="form.switcher.secondary.type"
                        class="w-full px-3 py-2 border border-slate-300 rounded-lg">
                    <option value="ATEM">ATEM</option>
                    <option value="vMix">vMix</option>
                    <option value="OBS">OBS</option>
                </select>
            </div>
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">IP Address</label>
                <input type="text" x-model="form.switcher.secondary.ip"
                       class="w-full px-3 py-2 border border-slate-300 rounded-lg"
                       placeholder="192.168.1.101">
            </div>
            <div>
                <label class="block text-sm font-medium text-slate-700 mb-1">Port</label>
                <input type="number" x-model="form.switcher.secondary.port"
                       class="w-full px-3 py-2 border border-slate-300 rounded-lg">
            </div>
            <div class="flex gap-2">
                <button @click="showSecondaryConfig = false"
                        class="flex-1 py-2 text-sm font-medium text-slate-600
                               border border-slate-300 rounded-lg hover:bg-slate-50">
                    Cancel
                </button>
                <button @click="saveSecondarySwitcher()"
                        class="flex-1 py-2 text-sm font-medium text-white
                               bg-purple-600 rounded-lg hover:bg-purple-700">
                    Save
                </button>
            </div>
        </div>
    </div>
</div>
```

---

## 8. 참고 문서

- 예제 구현: `/home/prod/tally-node/examples/1/components/interface_web/www/`
- UI 기능 명세: `/home/prod/tally-node/web/docs/UI_FEATURES.md`
- Alpine.js 문서: https://alpinejs.dev/
- Tailwind CSS 문서: https://tailwindcss.com/
