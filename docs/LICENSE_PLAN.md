# 라이선스 인증 기능 설계 (Device Limit 기반)

**작성일**: 2026-01-03
**버전**: 2.2

## 변경사항
- v2.2 (2026-01-03): 구현 진행상황 추가, 이벤트 기반 아키텍처 변경 계획 추가
- v2.1 (2026-01-03): 30분 device_limit 유예 기간 추가, 최대 20개 device_id 저장, LIFO 기능정지
- v2.0 (2026-01-03): Device Limit 기반 라이선스 시스템 설계

---

## 개요

ESP32-S3 Tally Node의 **TX(송신기) 라이선스 인증**으로, 온라인 검증을 통해 **Device Limit(연결 가능 RX 대수)**를 받아옵니다.

**핵심: 라이선스 인증의 목적은 `device_limit` 값을 받아오는 것**

---

## 1. 요구사항

### 1.1 라이선스 인증 방식

| 항목 | 내용 |
|------|------|
| 검증 방식 | 온라인 HTTP API |
| 라이선스 키 | 16자리 문자열 |
| MAC 바인딩 | 있음 (서버에서 MAC 확인) |
| **핵심 반환값** | **device_limit** (연결 가능 RX 대수) |

### 1.2 동작 제한

| 상태 | 설명 |
|------|------|
| 등록 완료 | `device_limit > 0`, 정상 작동 |
| 미등록 | `device_limit == 0`, 30분 유예 후 기능 제한 |
| **기본 device_limit** | **0** (미등록 상태) |

### 1.3 Device Limit 동작

| 단계 | 동작 |
|------|------|
| 1 | 웹 설정에서 라이선스 키 입력 → 서버 인증 → `device_limit` 저장 |
| 2 | 부팅 시 NVS에서 `device_limit` 로드 |
| 3 | RX가 상태응답 보내면 TX가 `device_id` 수집 (중복 시 스킵) |
| 4 | 30분 유예 후, 수집된 `device_id` 개수가 `device_limit` 초과 시 |
| 5 | **초과분 RX에게 기능정지 패킷 전송** |

---

## 2. 서버 API

**서버 주소**: `https://tally-node.gobongs.com`

### 2.1 라이선스 검증 (주요)

```http
POST /api/validate-license

Headers:
  Content-Type: application/json
  X-API-Key: tally-node-api-2025

Body:
{
  "license_key": "1234567890123456",
  "mac_address": "AC:67:B2:EA:4B:12"
}

Response (성공 - 최초 할당):
{
  "success": true,
  "message": "license가 성공적으로 디바이스에 할당되고 활성화되었습니다.",
  "license": {
    "id": 15,
    "license_key": "1234567890123456",
    "mac_address": "AC:67:B2:EA:4B:12",
    "device_limit": 16
  }
}

Response (성공 - 이미 할당됨):
{
  "success": true,
  "message": "license 검증 성공",
  "license": {
    "id": 15,
    "license_key": "1234567890123456",
    "mac_address": "AC:67:B2:EA:4B:12",
    "device_limit": 16
  }
}

Response (실패):
{
  "success": false,
  "error": "유효하지 않은 license입니다."
}

Response (다른 디바이스에 할당됨):
{
  "success": false,
  "error": "이 license는 다른 디바이스에 할당되어 있습니다."
}
```

### 2.2 라이선스 등록 (관리자용)

```http
POST /api/register-license

Headers:
  Content-Type: application/json
  X-API-Key: tally-node-api-2025

Body:
{
  "license_key": "1234567890123456",
  "name": "홍길동",
  "phone": "010-1234-5678",
  "email": "user@example.com",
  "device_limit": 16
}

Response:
{
  "success": true,
  "message": "license가 성공적으로 등록되었습니다.",
  "license_id": 15
}
```

### 2.3 라이선스 검색 (사용자용)

```http
POST /api/search-license

Headers:
  Content-Type: application/json
  X-API-Key: tally-node-api-2025

Body:
{
  "name": "홍길동",
  "phone": "010-1234-5678",
  "email": "user@example.com"
}

Response:
{
  "success": true,
  "count": 1,
  "licenses": [
    {
      "license_key": "1234567890123456",
      "device_limit": 16,
      "is_active": true
    }
  ]
}
```

### 2.4 서버 상태 확인

```http
GET /api/status

Response:
{
  "status": "OK",
  "timestamp": "2026-01-03T03:39:18.011Z",
  "message": "API가 정상적으로 작동 중입니다."
}
```

### 2.5 연결 테스트

```http
GET /api/connection-test

Headers:
  X-API-Key: tally-node-api-2025

Response:
{
  "success": true,
  "message": "API 연결이 성공적으로 확인되었습니다.",
  "timestamp": "2026-01-03T03:39:18.211Z",
  "api_key_valid": true,
  "server_status": "OK"
}
```

---

## 3. 아키텍처

### 3.1 컴포넌트 구조

```
03_service/license_service/
├── include/license_service.h
└── license_service.cpp

04_driver/license_client/
├── include/license_client.h
└── license_client.cpp
```

### 3.2 데이터 흐름

```
┌─────────────┐    HTTP POST    ┌──────────────┐
│ LicenseService │ ───────────────> │ License Server │
└─────────────┘                   └──────────────┘
       │                                  │
       │  device_limit                   │
       ▼                                  │
┌─────────────┐                          │
│     NVS     │                          │
│ (device_limit) │                        │
└─────────────┘                          │
       │                                  │
       ▼                                  │
┌─────────────────────────────────────┐  │
│   DeviceManager (RX device_id 관리)  │  │
│   - 등록된 device_id 리스트           │  │
│   - device_limit 초과 체크            │  │
└─────────────────────────────────────┘  │
       │                                  │
       ▼                                  │
┌─────────────────────────────────────┐  │
│   LoRaService                        │  │
│   - 기능정지 패킷 전송                │  │
└─────────────────────────────────────┘  │
```

---

## 4. 상태 관리

### 4.1 라이선스 상태

```cpp
typedef enum {
    LICENSE_STATE_VALID,        // 인증됨 (device_limit 있음)
    LICENSE_STATE_INVALID,      // 인증 실패
    LICENSE_STATE_GRACE,        // 유예 기간 (5분)
    LICENSE_STATE_LOCKED,       // 잠금 (스위처 차단)
    LICENSE_STATE_CHECKING      // 검증 중
} license_state_t;
```

### 4.2 NVS 저장 구조

```cpp
typedef struct {
    char license_key[17];       // 16자리 + null
    uint8_t device_limit;       // 디바이스 제한 (기본값: 0)
    uint32_t grace_start;       // 유예 기간 시작 시간 (epoch)
    uint8_t activated;          // 인증 여부
} license_config_t;
```

### 4.3 RX Device ID 관리

```cpp
// lora_protocol.h
#define LORA_DEVICE_ID_LEN     2  // 2바이트 (MAC[4] + MAC[5])

// config_service.h (기존 구조체 활용)
#define CONFIG_MAX_REGISTERED_DEVICES 20

typedef struct {
    uint8_t device_ids[CONFIG_MAX_REGISTERED_DEVICES][LORA_DEVICE_ID_LEN];  // 2바이트 device_id
    uint8_t count;                    // 현재 저장된 개수 (최대 20)
} config_registered_devices_t;

// DeviceManager에서 관리
#define MAX_STORED_DEVICES 20    // 최대 저장 개수 (limit 무관)
#define DEVICE_GRACE_PERIOD_SEC  (30 * 60)  // 30분 유예 시간

typedef struct {
    config_registered_devices_t devices;  // 등록된 RX device_id
    uint8_t device_limit;                 // 라이선스 제한
    uint32_t grace_start_time;            // 유예 시작 시간 (epoch)
    bool     grace_period_active;         // 유예 기간 활성화 여부
} device_registry_t;
```

### 4.4 Device Limit 체크 로직

| 조건 | 동작 |
|------|------|
| 유예 기간 내 (30분) | 모든 device 정상 작동, device_id 저장 (최대 20개) |
| 유예 기간 종료 후 | `device_limit` 초과분에 대해 기능정지 |
| 저장 공간 | 최대 20개 (limit 무관, 항상 최신 20개 유지) |
| 기능정지 순서 | 가장 마지막에 추가된 device부터 (LIFO) |

---

## 5. 동작 시나리오

### 5.1 정상 인증

```
[초기 설정 - 한 번만 수행]
1. 웹 설정 화면에서 license_key 입력
2. license_client_validate() → 서버에 license_key + mac_address 전송
3. 검증 성공 → device_limit 수신 (예: 16)
4. device_limit를 NVS에 저장
5. LICENSE_STATE_VALID

[부팅 시 - 매번 수행]
1. NVS에서 device_limit 로드
2. device_limit > 0 ?
   - YES: LICENSE_STATE_VALID, SwitcherService 정상 초기화
   - NO:  LICENSE_STATE_INVALID, 유예/잠금 처리
```

### 5.2 RX 연결 및 Device Limit 체크

```
1. RX 전원 켜짐 → 상태응답 패킷 전송 (device_id 포함)
2. TX 수신 → device_id 추출

3. 중복 체크:
   - 이미 저장된 device_id이면 → NVS 쓰기 스킵 (Flash 수명 보호)
   - 새로운 device_id이면 → NVS에 저장

4. 유예 기간 (30분) 확인:
   - 유예 내: 정상 동작, Tally 패킷 전송, 웹에 device_id 표시
   - 유예 종료:
     a. stored_count <= device_limit ?
        - YES: 정상 동작
        - NO:  device_limit 초과분에 기능정지 패킷 전송

4. 기능정지 순서: 가장 마지막에 추가된 device부터 (LIFO)

예시 (device_limit = 5):
- 유예 내: 10개의 RX 연결 → 모두 정상 작동
- 유옵 후: 마지막 5개 (idx 5~9)에게 기능정지 패킷 전송
```

### 5.3 기능정지 패킷 전송

```
조건: 유예 종료 AND stored_count > device_limit

TX 동작:
1. device_limit 초과분 device_id 확인 (인덱스 높은 순)
2. LoRa로 기능정지 패킷 전송 (헤더: 0xFF)
3. 디스플레이에 팝업 메시지

RX 동작 (기능정지 패킷 수신 시):
1. WS2812 LED 기능 정지
2. 화면에 "LICENSE REQUIRED" 팝업 표시
```

### 5.4 인증 실패 (미등록)

```
1. 부팅 → NVS에서 device_limit 로드
2. device_limit == 0 ? (미등록 상태)
   - YES: LICENSE_STATE_INVALID
   - NO:  LICENSE_STATE_VALID
3. 미등록 시 동작:
   - 30분 유예 기간 동작
   - 30분 후 기능 제한
```

---

## 6. LoRa 패킷 프로토콜

### 6.1 Device ID 형식

```cpp
// lora_protocol.h
#define LORA_DEVICE_ID_LEN     2  // 2바이트 (MAC[4] + MAC[5])
```

| 항목 | 값 | 예시 |
|------|-----|------|
| 생성 방식 | MAC[4] + MAC[5] 전체 | - |
| 내부 저장 | `uint8_t[2]` 또는 `uint16_t` | `{0x4B, 0x12}` |
| NVS 저장 | 2바이트 | - |
| 디스플레이 | 4자리 hex 문자열 | `"4B12"` |

**생성 코드:**
```cpp
// hardware_service.cpp
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);

s_device_id[0] = "0123456789ABCDEF"[mac[4] >> 4];  // MAC[4] 상위 4비트
s_device_id[1] = "0123456789ABCDEF"[mac[4] & 0x0F]; // MAC[4] 하위 4비트
s_device_id[2] = "0123456789ABCDEF"[mac[5] >> 4];  // MAC[5] 상위 4비트
s_device_id[3] = "0123456789ABCDEF"[mac[5] & 0x0F]; // MAC[5] 하위 4비트
s_device_id[4] = '\0';
// 결과: MAC `...4B:12` → "4B12"
```

### 6.2 기존 패킷 헤더 (lora_protocol.h)

```cpp
// Tally 데이터
#define LORA_HDR_TALLY_8CH     0xF1   // 8채널
#define LORA_HDR_TALLY_12CH    0xF2   // 12채널
#define LORA_HDR_TALLY_16CH    0xF3   // 16채널
#define LORA_HDR_TALLY_20CH    0xF4   // 20채널

// TX → RX 명령
#define LORA_HDR_STATUS_REQ    0xE0   // 상태 요청
#define LORA_HDR_SET_BRIGHTNESS 0xE1  // 밝기 설정
#define LORA_HDR_SET_CAMERA_ID 0xE2   // 카메라 ID 설정
#define LORA_HDR_SET_RF        0xE3   // RF 설정
#define LORA_HDR_STOP          0xE4   // 기능 정지 (라이선스 초과 시 사용)
#define LORA_HDR_REBOOT        0xE5   // 재부팅
#define LORA_HDR_PING          0xE6   // 지연시간 테스트

// RX → TX 응답
#define LORA_HDR_STATUS        0xD0   // 상태 정보
#define LORA_HDR_ACK           0xD1   // 명령 승인
#define LORA_HDR_PONG          0xD2   // PING 응답
```

### 6.3 기능정지 패킷 (기존 0xE4 사용)

```cpp
// lora_protocol.h (기존 정의된 것 그대로 사용)
#define LORA_HDR_STOP          0xE4   // 기능 정지

typedef struct __attribute__((packed)) {
    uint8_t header;                        // 0xE4
    uint8_t device_id[LORA_DEVICE_ID_LEN]; // 2바이트 device_id (MAC[4]+MAC[5])
} lora_cmd_stop_t;
```

### 6.4 RX 상태응답 패킷 (기존 0xD0 사용)

```cpp
typedef struct __attribute__((packed)) {
    uint8_t header;                        // 0xD0
    uint8_t device_id[LORA_DEVICE_ID_LEN]; // 2바이트 device_id (MAC[4]+MAC[5])
    uint8_t battery;                       // 0-100
    uint8_t camera_id;
    uint32_t uptime;                       // 초
    uint8_t brightness;                    // 0-100
    uint16_t frequency;                    // 현재 주파수 (MHz, 정수)
    uint8_t sync_word;                     // 현재 sync word
} lora_msg_status_t;
```

---

## 7. 동작 흐름도

```
┌─────────────────────────────────────────────────────────────┐
│ [초기 설정 - 한 번만]                                       │
│  웹 설정 화면 → license_key 입력                             │
│  → 서버에 license_key + mac_address 전송                     │
│  → device_limit 수신 (예: 16)                               │
│  → NVS에 저장                                               │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ TX 부팅 (매번)                                               │
│  → NVS에서 device_limit 로드 (기본값: 0)                     │
│  → device_limit > 0 ? VALID : INVALID                       │
│  → 30분 유예 타이머 시작                                     │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ RX 부팅                                                     │
│  → 상태응답 패킷 전송 (device_id 포함)                        │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ TX 수신                                                     │
│  → device_id 추출                                           │
│  → device_id 저장 (최대 20개)                                │
│  → 웹에 device_id 리스트 표시                                │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 유예 기간 확인 (30분)                                        │
└─────────────────────────────────────────────────────────────┘
                           │
            ┌──────────────┴──────────────┐
            ▼ 유예 내                       ▼ 유예 종료
┌──────────────────────┐     ┌──────────────────────────┐
│ 모든 device 정상 작동  │     │ stored_count 체크        │
│ Tally 패킷 전송       │     └──────────────────────────┘
└──────────────────────┘                 │
                                          ▼
                           ┌──────────────────────────┐
                           │ stored_count ≤ limit ?   │
                           └──────────────────────────┘
                                    │
                     ┌──────────────┴──────────────┐
                     ▼ YES                          ▼ NO
┌──────────────────────┐     ┌──────────────────────────┐
│ 정상 Tally 패킷 전송  │     │ 마지막 device부터        │
│ (모든 device)         │     │ 기능정지 패킷 전송        │
└──────────────────────┘     │ (헤더: 0xFF, LIFO)       │
                             └──────────────────────────┘
                                        │
                                        ▼
                             ┌──────────────────────────┐
                             │ RX 수신                   │
                             │  → WS2812 LED OFF         │
                             │  → 화면에 "LICENSE" 표시   │
                             └──────────────────────────┘
```

---

## 8. 디스플레이

### 8.1 TX 팝업 메시지 (device_limit 초과 시)

```
┌─────────────────────────────────┐
│                                 │
│  [!] 등록되지 않은 device        │
│      TX license 확인 필요        │
│                                 │
└─────────────────────────────────┘
```

### 8.2 RX 화면 (기능정지 패킷 수신 시)

```
┌─────────────────────────────────┐
│                                 │
│    [!] LICENSE REQUIRED         │
│                                 │
│    Device ID: 0xABCD            │
│                                 │
└─────────────────────────────────┘
```

### 8.3 타이밍 (TX 유예 기간)

| 구간 | 표시 여부 | 지속 시간 |
|------|----------|----------|
| 0~5초 | OFF | 5초 |
| 5~10초 | **LICENSE REQUIRED** | 5초 |
| 10~35초 | OFF | 25초 |
| 35~40초 | **LICENSE REQUIRED** | 5초 |
| ... | 30초 주기 반복 | ... |

---

## 9. SwitcherService 제한

### 9.1 라이선스 체크 전후

```cpp
// prod_tx_app.cpp
if (!license_service_is_valid()) {
    T_LOGE(TAG, "License invalid - switcher blocked");
    display_manager_show_license_page(true);
    return false;
}
switcher_service_create();
```

### 9.2 Tally 송신 차단

```cpp
// license_service.cpp
bool license_service_can_send_tally(void) {
    return (s_state == LICENSE_STATE_VALID) ||
           (s_state == LICENSE_STATE_GRACE);
}
```

---

## 10. 구현 현황

| 항목 | 상태 | 비고 |
|------|------|------|
| **LicenseClient** (04_driver) | ✅ 완료 | HTTP 통신, JSON 파싱, 연결 테스트 |
| **LicenseService** (03_service) | ✅ 완료 | NVS 직접 관리, 30분 유예 기간, 상태 관리 |
| **DeviceManager** | ⚠️ 부분 완료 | device_limit 체크 로직 추가 (아키텍처 이슈 있음) |
| **LoRaService** | ⏳ 예정 | 기능정지 패킷 (0xE4) 전송 |
| **LicensePage** | ⏳ 예정 | 디스플레이 UI |
| **prod_tx_app** | ⏳ 예정 | 초기화 순서 변경 |
| **web_server** | ⏳ 예정 | 라이선스 입력 API |
| **RX 펌웨어** | ⏳ 예정 | 기능정지 패킷 수신 처리 |

### 10.1 완료된 구현 상세

#### LicenseClient (04_driver)
```cpp
// components/04_driver/license_client/
├── include/license_client.h
├── license_client.cpp
└── CMakeLists.txt
```
- `license_client_validate()`: 라이선스 검증 API 호출
- `license_client_connection_test()`: 서버 연결 테스트
- `license_client_is_connected()`: WiFi 연결 상태 확인

#### LicenseService (03_service)
```cpp
// components/03_service/license_service/
├── include/license_service.h
├── license_service.cpp
└── CMakeLists.txt
```
- `license_service_init()`: NVS 초기화, 라이선스 정보 로드
- `license_service_validate()`: 라이선스 검증 요청
- `license_service_get_device_limit()`: device_limit 반환
- `license_service_is_valid()`: 유효성 확인 (VALID 또는 GRACE 상태)
- `license_service_get_state()`: 현재 상태 반환
- `license_service_is_grace_active()`: 유예 기간 확인
- `license_service_get_grace_remaining()`: 유예 기간 남은 시간
- `license_service_can_send_tally()`: Tally 전송 가능 여부

**NVS 직접 관리**:
- `NVS_NAMESPACE = "license"` 로 독립 네임스페이스 사용
- ConfigService를 통하지 않고 직접 NVS 접근 (서비스 간 직접 호출 방지)

### 10.2 아키텍처 이슈 및 변경 계획

#### 현재 문제점
```
DeviceManager (03_service)
    ↓ 직접 호출 (아키텍처 위반)
LicenseService (03_service)
```

**문제**: 서비스 레이어 간 직접 호출은 5계층 아키텍처 규칙 위반

#### 이벤트 기반 변경 계획
```
LicenseService (03_service)
    ↓ EVT_LICENSE_LIMIT_CHANGED 이벤트 발행
EventBus (00_common)
    ↓ 이벤트 전달
DeviceManager (03_service)
    ↓ 이벤트 구독, 내부 캐시 업데이트
device_limit (로컬 캐시)
```

#### 이벤트 정의 (event_bus.h에 추가)
```cpp
// 라이선스 상태 변경 이벤트
typedef struct {
    uint8_t device_limit;      // 0 = 미등록, 1~255 = 제한
    license_state_t state;     // 현재 라이선스 상태
    uint32_t grace_remaining;  // 유예 기간 남은 시간 (초)
} license_state_event_t;

// 이벤트 타입
#define EVT_LICENSE_STATE_CHANGED    EVT_LICENSE_BASE + 0  // 라이선스 상태 변경
#define EVT_LICENSE_LIMIT_CHANGED    EVT_LICENSE_BASE + 1  // device_limit 변경
```

#### DeviceManager 변경 사항
```cpp
// device_manager.cpp

// 내부 캐시 추가
static struct {
    uint8_t device_limit;       // license_service에서 이벤트로 수신
    bool limit_valid;           // 캐시 유효성 플래그
    // ...
} s_tx = {
    .device_limit = 0,          // 기본값: 무제한 (체크 안함)
    .limit_valid = false,
};

// 이벤트 핸들러
static esp_err_t on_license_state_changed(const event_data_t* event)
{
    const license_state_event_t* license =
        (const license_state_event_t*)event->data;

    s_tx.device_limit = license->device_limit;
    s_tx.limit_valid = true;

    T_LOGI(TAG, "license 상태 변경: limit=%d, state=%d",
             license->device_limit, license->state);

    return ESP_OK;
}

// 초기화 시 이벤트 구독
esp_err_t device_manager_start(void)
{
    // ...
    event_bus_subscribe(EVT_LICENSE_STATE_CHANGED, on_license_state_changed);
    // ...
}

// on_status_response에서 직접 호출 대신 캐시 사용
static void on_status_response(...)
{
    // ...
    uint8_t device_limit = s_tx.device_limit;  // 캐시된 값 사용

    if (device_limit == 0) {
        // 라이선스 미등록: 체크 안함
    } else if (s_tx.device_count > device_limit) {
        // 초과 시 처리
    }
}
```

### 10.3 NVS 관리 변경사항

**문서 13.9항 "NVS 접근 규칙" 변경**:
- ~~ConfigService 통해 접근~~ (기존 계획)
- **LicenseService가 NVS 직접 관리** (실제 구현)

**이유**:
- ConfigService도 서비스 레이어 (03_service)
- 서비스 간 직접 호출 방지를 위해 독립 NVS 네임스페이스 사용
- `license_service` 네임스페이스로 독립적 데이터 관리

---

## 11. 구현 순서 (수정)

| 단계 | 항목 | 상태 | 비고 |
|------|------|------|------|
| 1 | **LicenseClient** (04_driver) | ✅ 완료 | HTTP 통신, JSON 파싱 |
| 2 | **LicenseService** (03_service) | ✅ 완료 | NVS 직접 관리, 상태 관리 |
| 3 | **이벤트 기반 변경** | ⏳ 예정 | LicenseService 이벤트 발행, DeviceManager 구독 |
| 4 | **DeviceManager** 수정 | ⚠️ 부분 | 직접 호출 → 이벤트 기반으로 변경 필요 |
| 5 | **LoRaService** 수정 | ⏳ 예정 | 기능정지 패킷 (0xE4) 전송 |
| 6 | **LicensePage** (02_presentation) | ⏳ 예정 | 디스플레이 UI |
| 7 | **prod_tx_app** 수정 | ⏳ 예정 | 초기화 순서 변경, 라이선스 체크 |
| 8 | **web_server** 수정 | ⏳ 예정 | 라이선스 입력 API |
| 9 | **RX 펌웨어** 수정 | ⏳ 예정 | 기능정지 패킷 수신 처리 |

### 11.1 이벤트 기반 변경 세부 단계

1. **event_bus.h에 이벤트 정의**
   - `EVT_LICENSE_STATE_CHANGED` 추가
   - `license_state_event_t` 구조체 정의

2. **LicenseService 수정**
   - `validate()` 성공 시 이벤트 발행
   - `init()` 시 기존 상태로 이벤트 발행

3. **DeviceManager 수정**
   - `license_service` REQUIRES 제거
   - 내부 `device_limit` 캐시 추가
   - `on_license_state_changed()` 핸들러 추가
   - 이벤트 구독 추가

---

## 12. 이벤트 정의

```cpp
// event_bus.h
EVT_LICENSE_VALID             // 라이선스 검증 성공
EVT_LICENSE_INVALID           // 라이선스 검증 실패
EVT_LICENSE_GRACE_START       // 유예 기간 시작
EVT_LICENSE_LOCKED            // 잠금 (5분 경과)
EVT_DEVICE_LIMIT_EXCEEDED     // device_limit 초과
```

---

## 13. API 연결 정보

| 항목 | 값 |
|------|------|
| 서버 주소 | `https://tally-node.gobongs.com` |
| 검증 엔드포인트 | `/api/validate-license` |
| 등록 엔드포인트 | `/api/register-license` |
| 검색 엔드포인트 | `/api/search-license` |
| 상태 엔드포인트 | `/api/status` |
| 연결 테스트 | `/api/connection-test` |
| API 키 | `tally-node-api-2025` |
| 프로토콜 | HTTPS (443) |

---

## 14. 코딩 룰 (CLAUDE.md, ARCHITECTURE.md 기반)

### 14.1 언어 선택

| 계층 | 언어 | 라이선스 컴포넌트 |
|------|------|------------------|
| 05_hal | C | - |
| 04_driver | **C++** | license_client |
| 03_service | **C++** | license_service |
| 02_presentation | **C++** | LicensePage |
| 01_app | **C++** | prod_tx_app, prod_rx_app |

### 14.2 네이밍 규칙

| 항목 | 규칙 | 예시 |
|------|------|------|
| 함수명 | `snake_case` | `license_service_init()`, `license_client_validate()` |
| 상수 | `UPPER_SNAKE_CASE` | `MAX_STORED_DEVICES`, `DEVICE_GRACE_PERIOD_SEC` |
| 구조체/클래스 | `PascalCase` | `LicenseService`, `device_registry_t` |
| 타입 접미사 | `_t` | `license_config_t`, `license_state_t` |
| 주석 | 한글 | `// 라이선스 검증 요청` |

### 14.3 싱글톤 패턴 (C++)

```cpp
// license_service.h
class LicenseService {
public:
    static esp_err_t init(void);
    static esp_err_t validate(const char* key);
    static uint8_t get_device_limit(void);
    static bool is_valid(void);

private:
    LicenseService() = delete;  // 인스턴스화 방지
    ~LicenseService() = delete;
};

// extern "C" 인터페이스 (04_driver만 해당)
// license_client.h
#ifdef __cplusplus
extern "C" {
#endif

esp_err_t license_client_validate(const char* key, const char* mac);

#ifdef __cplusplus
}
#endif
```

### 13.4 의존성 규칙

```
O 올바른 의존:
01_app → license_service (03)
license_service → license_client (04)
license_client → esp_http_client (ESP-IDF)

X 잘못된 의존:
01_app → license_client (03 건너뛰기)
license_service → nvs_flash (ConfigService 통해서만)
```

### 13.5 이벤트 버스 통신

```cpp
// 발행 (Publish)
event_bus_publish(EVT_LICENSE_VALID, &device_limit, sizeof(uint8_t));

// 구독 (Subscribe)
event_bus_subscribe(EVT_LICENSE_VALID, on_license_valid);

// 핸들러
static void on_license_valid(void* handler_arg, esp_event_base_t base,
                               int32_t id, void* event_data) {
    uint8_t limit = *(uint8_t*)event_data;
    // 처리 로직
}
```

### 13.6 ESP-IDF 컴포넌트 사용

| 기능 | ESP-IDF 컴포넌트 | 용도 |
|------|------------------|------|
| HTTP 통신 | `esp_http_client` | 라이선스 서버 요청 |
| JSON 파싱 | `cJSON` | 응답 파싱 |
| 타이머 | `esp_timer` | 30분 유예 타이머 |
| NVS | `nvs_flash` | 라이선스 저장 (ConfigService 통해) |
| 로그 | `esp_log` | `ESP_LOGI(TAG, ...)` |
| 이벤트 | `esp_event` | event_bus 기반 |

### 13.7 CMakeLists.txt 패턴

```cmake
# 04_driver/license_client/CMakeLists.txt
idf_component_register(
    SRCS "license_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client cJSON
)
```

### 13.8 로그 패턴

```cpp
#include "esp_log.h"

static const char* TAG = "LicenseService";

ESP_LOGI(TAG, "라이선스 검증 시작: %s", key);
ESP_LOGW(TAG, "유예 기간 시작 (30분)");
ESP_LOGE(TAG, "라이선스 검증 실패: %s", error);
```

### 13.9 NVS 접근 규칙 (수정됨)

**서비스 간 직접 호출 방지를 위해 LicenseService가 NVS 직접 관리**

```cpp
// O 올바른 예: LicenseService가 독립 네임스페이스로 NVS 직접 관리
// components/03_service/license_service/license_service.cpp

#define NVS_NAMESPACE "license"

// LicenseService 내부 NVS 헬퍼
static esp_err_t nvsSetDeviceLimit(uint8_t limit);
static uint8_t nvsGetDeviceLimit(void);
static esp_err_t nvsSetLicenseKey(const char* key);
static esp_err_t nvsGetLicenseKey(char* key, size_t len);
```

**이유**:
- ConfigService도 서비스 레이어 (03_service)
- 서비스 간 직접 호출은 5계층 아키텍처 규칙 위반
- 독립 NVS 네임스페이스 `license`로 데이터 격리

**NVSC 저장 항목**:
| 키 | 타입 | 설명 |
|---|------|------|
| `license_key` | string | 라이선스 키 (16자리) |
| `device_limit` | uint8 | 디바이스 제한 (0=미등록) |
| `grace_start` | uint32 | 유예 기간 시작 시간 (epoch 초) |

### 13.10 NVS 쓰기 최적화 (Flash 수명 보호)

```cpp
// device_id 저장 시 중복 체크 필수
esp_err_t add_device_id(uint16_t device_id) {
    // 1. 먼저 NVS에서 현재 목록 로드
    config_registered_devices_t devices;
    config_service_get_registered_devices(&devices);

    // 2. 중복 체크
    for (uint8_t i = 0; i < devices.count; i++) {
        if (devices.device_ids[i] == device_id) {
            // 이미 저장됨 → NVS 쓰기 스킵 (Flash 수명 보호)
            return ESP_OK;
        }
    }

    // 3. 새로운 device_id만 저장
    return config_service_register_device(&device_id);
}
```

**중요:**
- Flash 쓰기 수명: 약 10,000~100,000 사이클
- 이미 저장된 device_id는 NVS 쓰기 스킵
- 변경 있을 때만 `nvs_commit()` 호출

### 13.11 파일 배치

```
components/03_service/license_service/
├── license_service.cpp      # 소스 (루트)
├── include/
│   └── license_service.h    # 헤더
└── CMakeLists.txt

components/04_driver/license_client/
├── license_client.cpp       # 소스 (루트)
├── include/
│   └── license_client.h     # 헤더
└── CMakeLists.txt
```

**중요:** `src/` 폴더를 사용하지 않음
