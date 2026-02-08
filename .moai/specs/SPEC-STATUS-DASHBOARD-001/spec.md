# SPEC-STATUS-DASHBOARD-001: Tally + 연결 상태 하이브리드 페이지

## TAG BLOCK

```
SPEC-ID: SPEC-STATUS-DASHBOARD-001
TITLE: Tally + 연결 상태 하이브리드 페이지
STATUS: Planned
PRIORITY: High
ASSIGNED: TBD
CREATED: 2025-02-08
DOMAIN: Presentation/Display
```

## 환경 (Environment)

### 하드웨어 제약사항

- **디스플레이**: SSD1306 OLED, 128x64 픽셀
- **폰트**: u8g2_font_profont11_mf (11px 높이, 11px 줄 간격)
- **좌표 시스템**: Baseline 기반, 최소 x=4px 패딩
- **렌더링**: U8g2 라이브러리 with framebuffer

### 소프트웨어 환경

- **프레임워크**: ESP-IDF v5.x
- **언어**: C/C++ (ESP-IDF 프로젝트)
- **아키텍처**: 이벤트 기반 서비스 아키텍처
- **디스플레이 관리자**: DisplayManager with 페이지 시스템

### 기존 페이지 구조

현재 TX 모드는 6개 페이지로 구성:
- **Page 1**: Tally 정보 (PGM/PVW 채널 목록) - **하이브리드 리팩토링 대상**
- **Page 2**: 스위처 정보 (S1, S2 듀얼 모드 지원)
- **Page 3**: AP 설정 (이름, 비밀번호, IP)
- **Page 4**: WiFi 설정 (SSID, 비밀번호, IP)
- **Page 5**: Ethernet 설정 (IP, 게이트웨이)
- **Page 6**: 시스템 정보 (주파수, 전압, 온도 등)

## 가정사항 (Assumptions)

### 기술적 가정사항

1. **서비스 가용성**: 모든 서비스 (network_service, switcher_service, battery_driver, tally_service)가 정상적으로 초기화되고 실행 중
2. **이벤트 기반 업데이트**: 상태 변경 시 이벤트를 통해 페이지 데이터 업데이트
3. **기존 아이콘 유지**: 배터리 및 신호 아이콘은 기존 icons.h 구현 재사용
4. **폰트 제약**: profont11_mf 외 다른 폰트 사용 불가 (공간/메모리 제약)
5. **페이지 네비게이션**: 기존 페이지 전환 메커니즘 유지

### 사용자 가정사항

1. **즉시 상태 파악**: 사용자는 전원 켜기 즉시 Tally 상태와 연결 상태를 한눈에 파악하고자 함
2. **Tally 정보 우선**: Tally 정보는 여전히 중요하며, 연결 상태와 함께 표시되어야 함
3. **IP 주소 불필요**: IP 주소는 다른 페이지에서 확인 가능하며, 첫 페이지에서는 제거
4. **단순한 시각적 언어**: 체크마크(✓)와 엑스마크(✗)로 연결 상태 직관적 표현 선호

## 요구사항 (Requirements)

### EARS 형식 요구사항

#### 1. Ubiquitous Requirements (상시 요구사항)

**REQ-001**: 시스템은 **항상** 첫 페이지에 헤더 영역을 표시해야 한다.
- 헤더에는 "DASHBOARD" 텍스트(좌측), 페이지 표시(중앙), 배터리 아이콘(우측)이 포함된다
- "DASHBOARD" 텍스트는 좌측(x=4, y=10)에 표시된다
- 배터리 아이콘은 우측 상단(x=105, y=3)에 표시되며 현재 배터리 레벨(0-3)을 시각화한다
- 페이지 표시는 중앙(x=55, y=10)에 "X/Y" 형식으로 표시된다
- 헤더 형식: "DASHBOARD          1/6 [BAT]" (좌측 텍스트 + 중앙 페이지 번호 + 우측 배터리)

**REQ-002**: 시스템은 **항상** profont11_mf 폰트를 사용하여 텍스트를 렌더링해야 한다.
- 모든 텍스트는 11px 높이, 11px 줄 간격으로 배치된다
- 최소 x=4px 패딩이 적용된다

**REQ-003**: 시스템은 **항상** Tally 정보(PGM/PVW 채널 리스트)를 첫 페이지 상단에 표시해야 한다.
- PGM 채널 리스트를 표시한다
- PVW 채널 리스트를 표시한다
- 텍스트 크기를 조정하여 연결 상태 표시 공간을 확보한다

#### 2. Event-Driven Requirements (이벤트 기반 요구사항)

**REQ-004**: **WHEN** 네트워크 상태가 변경되면 **THEN** 시스템은 즉시 WiFi 및 Ethernet 연결 상태를 업데이트해야 한다.
- WiFi STA: 연결됨(체크)/연결 안 됨(엑스) 표시
- **IP 주소는 표시하지 않는다**
- Ethernet: LINK UP(체크)/LINK DOWN(엑스) 표시
- **IP 주소는 표시하지 않는다**

**REQ-005**: **WHEN** 스위처 연결 상태가 변경되면 **THEN** 시스템은 즉시 S1, S2 연결 상태를 업데이트해야 한다.
- Primary Switcher (S1): 연결됨(체크)/연결 안 됨(엑스) 표시
- Secondary Switcher (S2): 듀얼 모드 시 연결됨(체크)/연결 안 됨(엑스) 표시

**REQ-006**: **WHEN** 배터리 레벨이 변경되면 **THEN** 시스템은 즉시 헤더의 배터리 아이콘을 업데이트해야 한다.
- 레벨 0 (0-25%): 빈 배터리
- 레벨 1 (26-50%): 낮음
- 레벨 2 (51-75%): 중간
- 레벨 3 (76-100%): 충분

**REQ-007**: **WHEN** Tally 상태가 변경되면 **THEN** 시스템은 즉시 PGM/PVW 채널 리스트를 업데이트해야 한다.
- PGM 채널 변경 시 즉시 반영
- PVW 채널 변경 시 즉시 반영

#### 3. State-Driven Requirements (상태 기반 요구사항)

**REQ-008**: **IF** 듀얼 모드가 비활성화되어 있으면 **THEN** 시스템은 S2 상태를 표시하지 않아야 한다.
- S2 상태 표시 영역을 숨기거나 비워둔다
- 해당 공간을 다른 상태 정보에 활용할 수 있다

**REQ-009**: **IF** 네트워크 인터페이스가 감지되지 않으면 **THEN** 시스템은 해당 인터페이스 상태를 "미감지"로 표시해야 한다.
- Ethernet 하드웨어가 없는 경우: 미감지 상태 표시

#### 4. Unwanted Behavior Requirements (금지 동작 요구사항)

**REQ-010**: 시스템은 첫 페이지 렌더링 시 **스크롤이나 페이지네이션을 요구해서는 안 된다**.
- 모든 상태 정보(Tally + 연결 상태)는 단일 128x64 화면内에 표시되어야 한다
- 사용자가 버튼을 눌러 추가 정보를 확인하게 해서는 안 된다

**REQ-011**: 시스템은 **IP 주소를 첫 페이지에 표시해서는 안 된다**.
- WiFi STA IP 주소 표시 금지
- WiFi AP IP 주소 표시 금지
- Ethernet IP 주소 표시 금지
- IP 주소는 각각의 설정 페이지(3, 4, 5)에서만 확인

**REQ-012**: 시스템은 **4-Line 레이아웃으로 정보를 표시해야 한다**.
- Line 1 (y=14): PGM 정보 표시 ("PGM: 1,2,3,4" 형식)
- Line 2 (y=25): PVW 정보 표시 ("PVW: 5" 형식)
- Line 3 (y=36): AP, WiFi, ETH 상태 표시 ("AP:[ON]  WiFi:[✓]  ETH:[✓]" 형식)
- Line 4 (y=47): SINGLE/DUAL 모드와 스위처 상태 표시
- 각 라인은 11px 간격으로 배치된다

**REQ-013**: 시스템은 **AP (Access Point) 상태를 on/off로 표시해야 한다**.
- 활성화: "[ON]" 표시
- 비활성화: "[---]" 표시 (strike-through 또는 대시)
- 연결 상태 체크 없음 (단순 활성화 상태만 표시)

**REQ-014**: 시스템은 **WiFi STA 상태를 연결 상태로 표시해야 한다**.
- 연결됨: "[✓]" 표시
- 연결 안 됨: "[✗]" 표시
- 하드웨어 미감지/비활성화: "[---]" 표시

**REQ-015**: 시스템은 **Ethernet 상태를 LINK UP/DOWN으로 표시해야 한다**.
- LINK UP (연결됨): "[✓]" 표시
- LINK DOWN (연결 안 됨): "[✗]" 표시
- 하드웨어 미감지/비활성화: "[---]" 표시

**REQ-016**: 시스템은 **스위처 상태를 스위처 타입과 함께 표시해야 한다**.
- 스위처 타입 표시: ATEM, OBS, vMix (실제 타입에 따라)
- Single 모드: "SINGLE  [타입]:[상태]" 형식
- Dual 모드: "DUAL  [타입]1:[상태]  [타입]2:[상태]" 형식
- 상태 표시: 연결됨 [✓], 연결 안 됨 [✗]

#### 5. Optional Requirements (선택적 요구사항)

**REQ-013**: **가능하면** 상태 변경 시 아이콘 색상을 변경하여 시각적 피드백을 제공한다.
- 연결됨: 녹색 또는 밝은 색상
- 연결 안 됨: 적색 또는 어두운 색상
- 참고: SSD1306은 단색 디스플레이이므로 밝기/패턴으로 구별

## 명세 (Specifications)

### UI 레이아웃 설계 (4-Line Layout)

```
┌────────────────────────────────────────────────────┐
│ DASHBOARD          1/6         [BAT]              │ ← 헤더 (y=0-11)
├────────────────────────────────────────────────────┤
│ PGM: 1,2,3,4                                       │ ← Line 1 (y=14)
│ PVW: 5                                             │ ← Line 2 (y=25)
│ AP:[ON]  WiFi:[✓]  ETH:[✓]                        │ ← Line 3 (y=36)
│ SINGLE  ATEM:[✓]                                  │ ← Line 4 (y=47)
└────────────────────────────────────────────────────┘
```

**듀얼 모드 예시**:
```
┌────────────────────────────────────────────────────┐
│ DASHBOARD          1/6         [BAT]              │ ← 헤더 (y=0-11)
├────────────────────────────────────────────────────┤
│ PGM: 1,2,3,4                                       │ ← Line 1 (y=14)
│ PVW: 5                                             │ ← Line 2 (y=25)
│ AP:[ON]  WiFi:[✓]  ETH:[✓]                        │ ← Line 3 (y=36)
│ DUAL  ATEM1:[✓]  ATEM2:[✗]                        │ ← Line 4 (y=47)
└────────────────────────────────────────────────────┘
```

### 좌표 시스템 배치

- **헤더 영역**: y=0-11 (profont11_mf 기본, 11px 높이)
  - "DASHBOARD" 텍스트: x=4, y=10 (좌측)
  - 페이지 표시: x=55, y=10 (중앙, "X/Y" 형식)
  - 배터리 아이콘: x=105, y=3 (우측, icons.c 기존 위치)

- **4-Line 레이아웃**:
  - **Line 1**: y=14 - PGM 정보 ("PGM: 1,2,3,4")
  - **Line 2**: y=25 - PVW 정보 ("PVW: 5")
  - **Line 3**: y=36 - AP, WiFi, ETH 상태
  - **Line 4**: y=47 - SINGLE/DUAL + 스위처 상태

### 상태 표시 포맷

**Line 1: PGM 정보**
```
PGM: [채널 리스트]
```
- 예시: "PGM: 1,2,3,4", "PGM: 5", "PGM: 1,2,3,4,5,6" (최대 6개 채널)

**Line 2: PVW 정보**
```
PVW: [채널 리스트]
```
- 예시: "PVW: 5", "PVW: 1,2", "PVW: 3,4,5" (최대 6개 채널)

**Line 3: AP, WiFi, ETH 상태**
```
AP:[상태]  WiFi:[상태]  ETH:[상태]
```

AP (Access Point) 상태:
- 활성화: `[ON]`
- 비활성화: `[---]` (strike-through 또는 대시 3개)

WiFi STA 상태:
- 연결됨: `[✓]`
- 연결 안 됨: `[✗]`
- 하드웨어 미감지: `[---]`

Ethernet 상태:
- LINK UP (연결됨): `[✓]`
- LINK DOWN (연결 안 됨): `[✗]`
- 하드웨어 미감지: `[---]`

**Line 4: SINGLE/DUAL + 스위처 상태**

Single 모드:
```
SINGLE  [스위처타입]:[상태]
```
- 예시: "SINGLE  ATEM:[✓]", "SINGLE  OBS:[✗]", "SINGLE  vMix:[✓]"

Dual 모드:
```
DUAL  [스위처1타입]1:[상태]  [스위처2타입]2:[상태]
```
- 예시: "DUAL  ATEM1:[✓]  ATEM2:[✗]", "DUAL  OBS1:[✓]  vMix2:[✓]"

스위처 타입 표시:
- ATEM: "ATEM"
- OBS: "OBS"
- vMix: "vMix"
- 미연결: "NONE"

### 표시 규칙

1. **AP (Access Point)**:
   - 활성화: `[ON]` 표시
   - 비활성화: `[---]` 표시 (strike-through 또는 대시)
   - 연결 상태 체크 없음 (단순 on/off 상태만 표시)

2. **WiFi STA**:
   - 연결됨: `[✓]` 표시
   - 연결 안 됨: `[✗]` 표시
   - 하드웨어 미감지/비활성화: `[---]` 표시

3. **Ethernet (ETH)**:
   - LINK UP (연결됨): `[✓]` 표시
   - LINK DOWN (연결 안 됨): `[✗]` 표시
   - 하드웨어 미감지/비활성화: `[---]` 표시

4. **스위처 (Switcher)**:
   - Single 모드: "SINGLE  [타입]:[상태]"
   - Dual 모드: "DUAL  [타입]1:[상태]  [타입]2:[상태]"
   - 스위처 타입을 실제 타입으로 표시 (ATEM, OBS, vMix)
   - `[ON]`/`[---]` 등의 상태 표시 사용

5. **IP 주소 미표시**:
   - WiFi STA IP 주소 표시 금지
   - WiFi AP IP 주소 표시 금지
   - Ethernet IP 주소 표시 금지
   - IP 주소는 각각의 설정 페이지(3, 4, 5)에서만 확인

### 텍스트 크기 조절 전략

**PGM/PVW 텍스트 축약**:
- 전체: "PGM: 1,2,3,4,5" → "P:1,2,3,4,5" (공간 부족 시)
- 전체: "PVW: 1,2,3,4,5" → "V:1,2,3,4,5" (공간 부족 시)
- 또는 채널 수 제한: 최대 4개 채널만 표시

**연결 상태 라벨 축약**:
- 전체: "WiFi STA" → "WiFi"
- 전체: "Ethernet" → "ETH"
- 전체: "Switcher S1" → "ATEM S1" (스위처 타입으로 표시)

### 아이콘 정의

**체크마크 (✓)**: 연결됨/활성화 상태
- 8x8 픽셀 비트맵 또는 U8g2 기본 심볼 사용
- 위치: 각 라인 연결 상태 영역 시작

**엑스마크 (✗)**: 연결 안 됨/비활성화 상태
- 8x8 픽셀 비트맵 또는 U8g2 기본 심볼 사용
- 위치: 각 라인 연결 상태 영역 시작

**배터리 아이콘**: 기존 icons.c의 drawTallyBatteryIcon 재사용
- 20x8 픽셀, 충전 표시 포함
- 4단계 레벨 (0-3)

### 데이터 구조

```c
// AP 상태 코드
typedef enum {
    AP_STATUS_INACTIVE = 0,  // 비활성화 ([---])
    AP_STATUS_ACTIVE = 1     // 활성화 ([ON])
} ap_status_t;

// 네트워크 연결 상태 코드
typedef enum {
    NET_STATUS_NOT_DETECTED = 0,  // 하드웨어 미감지 ([---])
    NET_STATUS_DISCONNECTED = 1,  // 연결 안 됨 ([✗])
    NET_STATUS_CONNECTED = 2      // 연결됨 ([✓])
} network_status_t;

// 스위처 타입
typedef enum {
    SWITCHER_TYPE_NONE = 0,
    SWITCHER_TYPE_ATEM = 1,
    SWITCHER_TYPE_OBS = 2,
    SWITCHER_TYPE_VMIX = 3
} switcher_type_t;

// 하이브리드 페이지 데이터
typedef struct {
    // Tally 상태
    uint8_t pgm_channels[16];  // PGM 채널 리스트
    uint8_t pgm_channel_count;
    uint8_t pvw_channels[16];  // PVW 채널 리스트
    uint8_t pvw_channel_count;

    // WiFi 상태
    network_status_t wifi_sta_status;
    ap_status_t wifi_ap_status;

    // Ethernet 상태
    network_status_t eth_status;

    // Switcher 상태
    network_status_t s1_status;
    switcher_type_t s1_type;
    network_status_t s2_status;
    switcher_type_t s2_type;
    bool dual_mode;

    // 배터리 상태
    uint8_t battery_percent;
} hybrid_dashboard_data_t;
```

## 추적성 (Traceability)

### 관련 파일

- **구현 파일**:
  - `components/02_presentation/display/pages/TxPage/TxPage.cpp` (수정)
  - `components/02_presentation/display/pages/TxPage/include/TxPage.h` (수정)
  - `components/02_presentation/display/icons/icons.c` (아이콘 추가)
  - `components/02_presentation/display/icons/include/icons.h` (아이콘 선언 추가)

- **서비스 인터페이스**:
  - `components/03_service/network_service/include/network_service.h`
  - `components/03_service/switcher_service/include/switcher_service.h`
  - `components/03_service/tally_service/include/tally_service.h`
  - `components/04_driver/battery_driver/include/battery_driver.h`

- **이벤트 정의**:
  - `components/00_common/event_bus/include/event_bus.h`
  - `EVT_NETWORK_STATUS_CHANGED`: 네트워크 상태 변경
  - `EVT_SWITCHER_STATUS_CHANGED`: 스위처 상태 변경
  - `EVT_TALLY_STATUS_CHANGED`: Tally 상태 변경
  - `EVT_BATTERY_STATUS_CHANGED`: 배터리 상태 변경

### 의존성

- DisplayManager: 페이지 등록 및 렌더링
- U8g2 라이브러리: 저수준 디스플레이 제어
- profont11_mf 폰트: 텍스트 렌더링
- TallyService: PGM/PVW 채널 상태 제공

### 마일스톤 의존관계

- **사전 작업**: 없음 (독립적 리팩토링)
- **병행 작업**: 없음
- **후속 작업**: Web UI 상태 대시보드와의 통합 (선택적)
