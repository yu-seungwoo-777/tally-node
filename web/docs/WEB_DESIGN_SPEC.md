# Tally Web UI - 디자인 및 구현 스펙

이 문서는 Tally Node Web UI의 디자인 시스템, 컴포넌트 구조, 구현 방식을 설명합니다. 다른 프로젝트에서 유사한 UI를 구현할 때 참고할 수 있습니다.

---

## 1. 기술 스택

| 기술 | 버전 | 용도 |
|------|------|------|
| Alpine.js | v3 | 반응형 상태 관리, 컴포넌트 기반 아키텍처 |
| Tailwind CSS | v3.4 | 유틸리티 기반 스타일링 (CDN) |
| DaisyUI | v5.5 | UI 컴포넌트 라이브러리 |
| ESBuild | v0.20 | JavaScript 번들링 |
| PostCSS | v8.5 | CSS 처리 |

---

## 2. 프로젝트 구조

```
web/
├── src/
│   ├── index.html           # 메인 레이아웃 (사이드바, 헤더, 다이얼로그)
│   ├── pages/               # 페이지별 HTML 조각
│   │   ├── dashboard.html
│   │   ├── network.html
│   │   ├── switcher.html
│   │   ├── broadcast.html
│   │   ├── devices.html
│   │   ├── system.html
│   │   └── license.html
│   ├── js/
│   │   ├── app.js           # 엔트리 포인트, 모듈 조립
│   │   └── modules/         # 기능별 모듈
│   │       ├── state.js     # 상태 관리, 폴링
│   │       ├── network.js   # 네트워크 설정
│   │       ├── switcher.js  # 스위처 설정
│   │       ├── broadcast.js # 방송 설정
│   │       ├── devices.js   # 디바이스 관리
│   │       ├── device.js    # 단일 디바이스
│   │       ├── license.js   # 라이선스
│   │       └── utils.js     # 유틸리티 함수
│   ├── css/
│   │   ├── styles.css       # 커스텀 스타일
│   │   └── input.css        # Tailwind input 파일
│   ├── vendor/
│   │   └── alpine.js        # Alpine.js 라이브러리
│   └── icons/               # SVG 아이콘
├── dist/                    # 빌드 결과물
├── tools/                   # 빌드 스크립트
└── docs/                    # 문서
```

---

## 3. 디자인 시스템

### 3.1 색상 팔레트

#### 기본 색상 (Slate)
```
slate-50    #f8fafc   (배경)
slate-100   #f1f5f9   (카드 헤더, 비활성)
slate-200   #e2e8f0   (테두리)
slate-300   #cbd5e1   (입력 테두리)
slate-400   #94a3b8   (비활성 아이콘)
slate-500   #64748b   (보조 텍스트)
slate-600   #475569   (일반 텍스트)
slate-700   #334155   (강조 텍스트)
slate-900   #0f172a   (제목 텍스트)
```

#### 상태 색상
| 의미 | 배경 | 테두리 | 텍스트 | 아이콘 |
|------|------|--------|--------|-------|
| 성공 | emerald-50 (#ecfdf5) | emerald-200 (#a7f3d0) | emerald-600/700 (#059669) | emerald-500/600 |
| 에러 | red-50 (#fef2f2) | red-200 (#fecaca) | red-600/700 (#dc2626) | red-500/600 |
| 정보 | blue-50 (#eff6ff) | blue-200 (#bfdbfe) | blue-600/700 (#2563eb) | blue-500/600 |
| 경고 | amber-50 (#fffbeb) | amber-200 (#fde68a) | amber-600/700 (#d97706) | amber-500/600 |

#### 기능별 강조 색상
| 기능 | 색상 | Hex |
|------|------|-----|
| Primary (S1) | Blue | blue-50/100/600/700 |
| Secondary (S2) | Purple | purple-50/100/600/700 |
| AP 설정 | Violet | violet-50/100/600/700 |
| WiFi 설정 | Blue | blue-50/100/600/700 |
| Ethernet | Emerald | emerald-50/100/600/700 |
| 라이선스 | Indigo | indigo-50/100/600/700 |
| 인터넷 테스트 | Cyan | cyan-50/100/600/700 |
| 검색 | Amber | amber-50/100/600/700 |

### 3.2 타이포그래피

```
text-xs          0.75rem    (12px)  - 보조 정보
text-sm          0.875rem   (14px)  - 일반 텍스트, 입력
text-base        1rem       (16px)  - 기본
text-lg          1.125rem   (18px)  - 소제목 (모바일)
text-xl          1.25rem    (20px)  - 소제목 (데스크톱)
font-semibold    600        - 강조
font-bold        700        - 제목
```

모노스페이스 폰트는 IP 주소, Device ID 등에 사용:
```
font-mono        (ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas)
```

### 3.3 간격 시스템

```
p-2     0.5rem   (8px)
p-3     0.75rem  (12px)
p-4     1rem     (16px)   - 기본 패딩
p-6     1.5rem   (24px)
gap-2   0.5rem   (8px)
gap-3   0.75rem  (12px)   - 요소 간 간격
gap-4   1rem     (16px)
```

### 3.4 라운딩 (Border Radius)

```
rounded-lg      0.5rem   (8px)   - 입력 필드, 작은 카드
rounded-xl      0.75rem  (12px)  - 카드, 버튼 (기본)
rounded-2xl     1rem     (16px)  - 디바이스 카드
rounded-full    9999px   - 상태 인디케이터, 배지
```

### 3.5 그림자

```
shadow-sm       0 1px 2px 0 rgb(0 0 0 / 0.05)   - 카드
shadow-md       0 4px 6px -1px rgb(0 0 0 / 0.1) - 호버
shadow-xl       0 20px 25px -5px rgb(0 0 0 / 0.1) - 다이얼로그
```

---

## 4. 레이아웃 구조

### 4.1 전체 레이아웃

```
┌─────────────────────────────────────────────┐
│  [사이드바]                                  │
│  ┌────────────┐  ┌──────────────────────┐  │
│  │            │  │ [헤더]               │  │
│  │  네비게이션 │  │                      │  │
│  │            │  │                      │  │
│  │            │  ├──────────────────────┤  │
│  │            │  │                      │  │
│  │            │  │  [페이지 컨텐츠]     │  │
│  │            │  │                      │  │
│  │            │  │                      │  │
│  │  [연결상태]│  └──────────────────────┘  │
│  └────────────┘                            │
└─────────────────────────────────────────────┘
```

### 4.2 사이드바 (Desktop: Fixed, Mobile: Off-canvas)

```html
<!-- Desktop: static, Mobile: fixed + transform -->
<aside :class="sidebarOpen ? 'translate-x-0' : '-translate-x-full lg:translate-x-0'"
       class="fixed lg:static inset-y-0 left-0 z-50 w-64 bg-white border-r border-slate-200 transition-transform duration-300">
```

### 4.3 페이지 전환 방식

- **해시 기반 라우팅**: `#dashboard`, `#network`, `#switcher`, 등
- **Alpine.js 조건부 렌더링**: `x-show="currentView === 'dashboard'"`
- **트랜지션**: `x-transition:enter`, `x-transition:enter-start`, `x-transition:enter-end`

```html
<div x-show="currentView === 'dashboard'"
     x-transition:enter="transition ease-out duration-200"
     x-transition:enter-start="opacity-0 translate-y-2"
     x-transition:enter-end="opacity-100 translate-y-0">
```

---

## 5. 컴포넌트 가이드

### 5.1 카드 (Card)

기본 카드 구조:
```html
<div class="bg-white rounded-xl shadow-sm border border-slate-200 overflow-hidden">
    <!-- 카드 헤더 (선택사항) -->
    <div class="px-4 py-3.5 border-b border-slate-100 bg-gradient-to-r from-blue-50 to-transparent">
        <h2 class="font-semibold text-slate-900 text-base">카드 제목</h2>
    </div>
    <!-- 카드 컨텐츠 -->
    <div class="p-4">
        <!-- 내용 -->
    </div>
</div>
```

헤더 그라디언트 색상 (기능별):
- Primary: `from-blue-50`
- Secondary: `from-purple-50`
- AP: `from-violet-50`
- WiFi: `from-blue-50`
- Ethernet: `from-emerald-50`
- License: `from-indigo-50`
- Internet: `from-cyan-50`
- Search: `from-amber-50`

### 5.2 버튼 (Button)

기본 버튼:
```html
<button class="px-4 py-2.5 text-sm font-medium text-white bg-blue-600 rounded-xl hover:bg-blue-700 transition-colors">
    버튼 텍스트
</button>
```

버튼 variants:
| 타입 | 클래스 |
|------|--------|
| Primary | `bg-blue-600 hover:bg-blue-700` |
| Secondary | `bg-purple-600 hover:bg-purple-700` |
| Success | `bg-emerald-600 hover:bg-emerald-700` |
| Danger | `bg-rose-600 hover:bg-rose-700` |
| Ghost | `text-slate-600 border border-slate-300 hover:bg-slate-50` |

로딩 상태:
```html
<button :disabled="loading" class="...">
    <svg x-show="loading" class="w-4 h-4 animate-spin" fill="none" stroke="currentColor">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"></path>
    </svg>
    <span x-text="loading ? '처리 중...' : '저장'"></span>
</button>
```

### 5.3 입력 필드 (Input)

텍스트 입력:
```html
<div>
    <label class="block text-sm font-medium text-slate-700 mb-1">라벨</label>
    <input type="text" x-model="form.value"
           class="w-full px-3 py-2 border border-slate-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500 text-sm"
           placeholder="플레이스홀더">
</div>
```

비밀번호 입력:
```html
<input type="password" x-model="form.password" class="w-full px-3 py-2 border border-slate-300 rounded-lg" placeholder="Password">
```

체크박스:
```html
<label class="flex items-center gap-2 cursor-pointer">
    <input type="checkbox" x-model="form.enabled"
           class="w-4 h-4 text-blue-600 rounded focus:ring-blue-500 border-slate-300">
    <span class="text-sm font-medium text-slate-700">라벨</span>
</label>
```

### 5.4 드롭다운 (Custom Dropdown)

Alpine.js 기반 커스텀 드롭다운:
```html
<div class="relative" x-data="{ open: false }">
    <button @click="open = !open" @click.away="open = false"
            class="w-full px-3 py-2 border border-slate-300 rounded-lg text-left flex items-center justify-between bg-white">
        <span x-text="selectedOption"></span>
        <svg class="w-4 h-4 text-slate-500" :class="{ 'rotate-180': open }">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 9l-7 7-7-7"></path>
        </svg>
    </button>
    <div x-show="open" x-transition
         class="absolute z-10 w-full mt-1 bg-white border border-slate-200 rounded-lg shadow-lg">
        <template x-for="option in options">
            <div @click="selectOption(option); open = false"
                 class="px-3 py-2 text-sm hover:bg-blue-50 cursor-pointer"
                 :class="{ 'bg-blue-100': selected === option }"
                 x-text="option"></div>
        </template>
    </div>
</div>
```

### 5.5 상태 배지 (Status Badge)

연결 상태:
```html
<span class="flex items-center gap-1.5 text-xs px-2.5 py-1 rounded-full"
      :class="connected ? 'bg-emerald-100 text-emerald-700' : 'bg-rose-100 text-rose-700'">
    <span class="w-1.5 h-1.5 rounded-full" :class="connected ? 'bg-emerald-500' : 'bg-rose-500'"></span>
    <span x-text="connected ? 'Connected' : 'Disconnected'"></span>
</span>
```

타입 배지:
```html
<span class="text-xs px-2 py-0.5 rounded-md bg-blue-100 text-blue-700">ATEM</span>
```

### 5.6 토스트 (Toast Notification)

```html
<div class="toast toast-end z-50" x-show="toast.show" x-transition>
    <div class="toast-custom" :class="toast.type === 'alert-success' ? 'toast-success' : 'toast-error'">
        <svg class="w-5 h-5 shrink-0">...</svg>
        <span class="text-sm font-medium" x-text="toast.message"></span>
    </div>
</div>
```

Toast 타입:
- `toast-success`: emerald 색상
- `toast-error`: red 색상
- `toast-info`: violet 색상
- `toast-warning`: amber 색상

### 5.7 다이얼로그 (Modal)

```html
<div x-show="showDialog" x-cloak
     class="fixed inset-0 z-50 flex items-center justify-center bg-black/50"
     x-transition:enter="transition ease-out duration-200"
     x-transition:enter-start="opacity-0"
     x-transition:enter-end="opacity-100">
    <div class="bg-white rounded-xl shadow-xl w-full max-w-md mx-4"
         x-transition:enter="transition ease-out duration-200"
         x-transition:enter-start="opacity-0 scale-95"
         x-transition:enter-end="opacity-100 scale-100">
        <!-- 헤더 -->
        <div class="px-4 py-3 bg-blue-50 border-b border-blue-100 flex items-center justify-between">
            <h3 class="font-semibold text-slate-900">제목</h3>
            <button @click="showDialog = false">...</button>
        </div>
        <!-- 컨텐츠 -->
        <div class="p-4 space-y-4">...</div>
        <!-- 푸터 버튼 -->
        <div class="flex gap-2">
            <button @click="showDialog = false" class="flex-1 ...">취소</button>
            <button @click="save()" class="flex-1 ...">저장</button>
        </div>
    </div>
</div>
```

### 5.8 프로그레스 바 (Progress Bar)

```html
<div class="h-2 bg-slate-100 rounded-full overflow-hidden">
    <div class="h-full rounded-full transition-all duration-500 bg-gradient-to-r from-emerald-400 to-emerald-600"
         :style="`width: ${percent}%`"></div>
</div>
```

색상 variants:
- 좋음 (>50%): `from-emerald-400 to-emerald-600`
- 보통 (20-50%): `from-yellow-400 to-yellow-600`
- 나쁨 (<=20%): `from-rose-400 to-rose-600`

### 5.9 스탯 카드 (Hero Stats)

```html
<div class="bg-white rounded-xl p-4 shadow-sm border border-slate-200 hover:shadow-md transition-shadow">
    <div class="flex items-center justify-between">
        <div class="flex items-center gap-3">
            <div class="w-10 h-10 bg-emerald-50 rounded-xl flex items-center justify-center shrink-0">
                <svg class="w-5 h-5 text-emerald-600">...</svg>
            </div>
            <span class="text-sm text-slate-500">Battery</span>
        </div>
        <span class="text-xl md:text-2xl font-bold text-slate-900" x-text="battery + '%'"></span>
    </div>
</div>
```

아이콘 배경 색상:
- 배터리: `bg-emerald-50` + `text-emerald-600`
- 온도: `bg-orange-50` + `text-orange-600`
- 업타임: `bg-blue-50` + `text-blue-600`
- 전압: `bg-slate-100` + `text-slate-600`

### 5.10 디바이스 카드 (Device Card)

```html
<div class="bg-white rounded-2xl border overflow-hidden hover:shadow-lg hover:border-slate-300 transition-all duration-200">
    <!-- 헤더: 상태 인디케이터 (왼쪽 테두리) -->
    <div class="relative px-4 py-3">
        <div class="absolute left-0 top-0 bottom-0 w-1"
             :class="online ? 'bg-emerald-500' : 'bg-slate-300'"></div>
        <div class="flex items-center justify-between">
            <!-- 아이콘 + 정보 -->
            <div class="flex items-center gap-3">
                <div class="w-10 h-10 rounded-xl flex items-center justify-center">...</div>
                <div>
                    <p class="font-bold text-slate-900" x-text="deviceId"></p>
                    <div class="flex items-center gap-1.5">
                        <span class="text-xs text-slate-500">Camera</span>
                        <span class="px-1.5 py-0.5 text-xs font-bold rounded-md bg-blue-100 text-blue-700">1</span>
                    </div>
                </div>
            </div>
            <!-- Ping -->
            <p class="text-xs font-medium text-emerald-600" x-text="ping + ' ms'"></p>
        </div>
    </div>
    <!-- 컨텐츠: 배터리, 메트릭 -->
    <div class="p-4 space-y-4">...</div>
</div>
```

---

## 6. Alpine.js 아키텍처

### 6.1 모듈화된 컴포넌트

```javascript
// app.js - 엔트리 포인트
import { stateModule } from './modules/state.js';
import { networkModule } from './modules/network.js';
// ...

function tallyApp() {
    return {
        sidebarOpen: false,
        ...stateModule(),
        ...networkModule(),
        // ...
    };
}

document.addEventListener('alpine:init', () => {
    Alpine.data('tallyApp', tallyApp);
});
```

### 6.2 상태 관리 패턴

```javascript
export function stateModule() {
    return {
        // 초기화 플래그
        _initialized: false,

        // 상태 데이터
        currentView: 'dashboard',
        wsConnected: false,
        system: { deviceId: '0000', battery: 0, ... },

        // 설정 데이터 (읽기 전용 표시용)
        config: { ... },

        // 폼 데이터 (입력용)
        form: { ... },

        // 초기화
        async init() {
            await this.fetchStatus();
            this.startStatusPolling();
        },

        // API 호출
        async fetchStatus() {
            const res = await fetch('/api/status');
            const data = await res.json();
            // 상태 업데이트 (초기화 시에만 폼 초기화)
            if (!this._initialized) {
                this.form.xxx = data.xxx;
            }
            this._initialized = true;
        },

        // 폴링
        startStatusPolling() {
            setInterval(async () => {
                await this.fetchStatus();
            }, 2000);
        }
    };
}
```

### 6.3 해시 기반 라우팅

```javascript
// URL 해시에서 뷰 복원
const hash = window.location.hash.slice(1);
if (hash && ['dashboard', 'network', ...].includes(hash)) {
    this.currentView = hash;
}

// 해시 변경 감지
window.addEventListener('hashchange', () => {
    const newHash = window.location.hash.slice(1);
    if (newHash && [...].includes(newHash)) {
        this.currentView = newHash;
    }
});
```

### 6.4 페이지 컨텐츠 주입 방식

빌드 시 `pages/*.html` 파일의 내용이 `index.html`의 `PAGES_PLACEHOLDER` 주석으로 주입됩니다:

```html
<!-- index.html -->
<main>
    <!-- PAGES_PLACEHOLDER: 빌드 시 pages/*.html 내용으로 교체 -->
</main>
```

빌드 결과:
```html
<main>
    <div x-show="currentView === 'dashboard'" x-transition ...>...</div>
    <div x-show="currentView === 'network'" x-transition ...>...</div>
    ...
</main>
```

---

## 7. 반응형 디자인

### 7.1 브레이크포인트

```
sm:     640px    (모바일 가로)
md:     768px    (태블릿)
lg:     1024px   (데스크톱)
xl:     1280px   (큰 데스크톱)
```

### 7.2 그리드 시스템

```html
<!-- 1열 (모바일) → 2열 (데스크톱) -->
<div class="grid gap-4 md:grid-cols-2">

<!-- 1열 (모바일) → 2열 (태블릿) → 3열 (데스크톱) -->
<div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">

<!-- 3열 (모바일) -->
<div class="grid grid-cols-3 gap-4">
```

### 7.3 반응형 텍스트

```html
<h2 class="text-lg md:text-xl font-semibold">제목</h2>
<p class="text-xs md:text-sm">설명</p>
```

### 7.4 모바일 전용 표시/숨김

```html
<!-- 모바일에서만 표시 -->
<span class="sm:hidden">모바일</span>

<!-- 데스크톱에서만 표시 -->
<span class="hidden sm:block">데스크톱</span>
```

### 7.5 사이드바 반응형

```html
<!-- 모바일: 햄버거 메뉴 + 오버레이 -->
<button @click="sidebarOpen = true" class="lg:hidden">햄버거</button>
<div x-show="sidebarOpen" x-transition.opacity
     class="fixed inset-0 bg-slate-900/20 backdrop-blur-sm z-40 lg:hidden"></div>

<!-- 사이드바: 모바일은 off-canvas, 데스크톱은 static -->
<aside :class="sidebarOpen ? 'translate-x-0' : '-translate-x-full lg:translate-x-0'"
       class="fixed lg:static inset-y-0 left-0 z-50 w-64 transition-transform">
```

---

## 8. 애니메이션 및 트랜지션

### 8.1 페이지 전환

```html
x-transition:enter="transition ease-out duration-200"
x-transition:enter-start="opacity-0 translate-y-2"
x-transition:enter-end="opacity-100 translate-y-0"
```

### 8.2 토스트 슬라이드

```html
x-transition:enter="transition ease-out duration-300"
x-transition:enter-start="translate-x-full opacity-0"
x-transition:enter-end="translate-x-0 opacity-100"
x-transition:leave="transition ease-in duration-200"
x-transition:leave-start="translate-x-0 opacity-100"
x-transition:leave-end="translate-x-full opacity-0"
```

### 8.3 드롭다운 페이드

```html
x-show="open"
x-transition
```

### 8.4 로딩 스피너

```html
<svg class="w-4 h-4 animate-spin" fill="none" stroke="currentColor">
    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"></path>
</svg>
```

---

## 9. 아이콘

인라인 SVG 사용 (Heroicons 스타일):

```html
<svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
    <path d="..."></path>
</svg>
```

크기:
- `w-4 h-4`: 작은 아이콘 (버튼 내, 입력 인디케이터)
- `w-5 h-5`: 기본 아이콘
- `w-6 h-6`: 큰 아이콘

---

## 10. 빌드 프로세스

### 10.1 개발 서버

```bash
npm run dev  # http://localhost:8080
```

### 10.2 프로덕션 빌드

```bash
# 1. 소스 빌드 (dist/ 생성)
npm run build

# 2. ESP32 임베디드 파일 생성 (_h.h 파일)
npm run embed

# 3. ESP32 컴포넌트로 복사
npm run copy

# 전체 배포
npm run deploy
```

### 10.3 빌드 단계

1. **JS 번들링**: ESBuild로 `js/app.js` → `dist/js/app.bundle.js`
2. **CSS 처리**: PostCSS + Tailwind로 `css/styles.css` → `dist/css/styles.css`
3. **HTML 주입**: `pages/*.html` → `index.html`의 `PAGES_PLACEHOLDER`
4. **임베디드 파일**: CSS/JS를 C 헤더 파일로 변환

---

## 11. 구현 시 주의사항

### 11.1 Alpine.js 반응성

- 객체의 중첩 속성 변경은 반응하지 않을 수 있음
- 해결: 새 객체 생성 `this.obj = { ...this.obj, ...newData }`

### 11.2 x-cloak 디렉티브

Alpine.js 로딩 전 요소 숨김:
```css
[x-cloak] { display: none !important; }
```

### 11.3 스크롤바 숨김

```css
::-webkit-scrollbar { display: none; }
* { -ms-overflow-style: none; scrollbar-width: none; }
```

### 11.4 모바일 Toast 위치

```css
@media (max-width: 768px) {
    .toast.toast-end {
        right: 0.5rem;
        left: 0.5rem;
        bottom: 0.5rem;
    }
}
```

---

## 12. 일관된 컴포넌트 작성 가이드

### 12.1 카드 작성 순서

1. `bg-white rounded-xl shadow-sm border border-slate-200` 기본 클래스
2. `overflow-hidden`: 헤더가 있을 때
3. 헤더: `px-4 py-3.5 border-b border-slate-100` + 기능별 그라디언트
4. 컨텐츠: `p-4 space-y-4`

### 12.2 색상 사용 규칙

- Primary: Blue
- Secondary: Purple
- AP: Violet
- WiFi: Blue
- Ethernet: Emerald
- 각 기능에 맞는 색상을 일관되게 사용

### 12.3 상태 표시 규칙

- 연결/활성: `emerald`
- 비연결/비활성: `slate` 또는 `rose`
- 경고: `amber` 또는 `yellow`
- 에러: `rose`

---

## 13. API 통신 패턴

```javascript
async saveNetwork() {
    try {
        const response = await fetch('/api/network', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(this.form.wifi)
        });
        if (!response.ok) throw new Error('Save failed');
        this.showToast('success', 'WiFi settings saved');
    } catch (e) {
        this.showToast('error', 'Failed to save: ' + e.message);
    }
}
```

---

*이 문서는 Tally Node Web UI의 디자인과 구현 방식을 체계적으로 정리한 것입니다. 새 페이지를 추가하거나 기존 페이지를 수정할 때 이 가이드를 참고하여 일관성을 유지하세요.*
