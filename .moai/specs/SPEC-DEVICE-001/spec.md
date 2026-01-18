# SPEC-DEVICE-001: RX 장치 기능정지 상태 감지 및 복구

## TAG BLOCK

```
SPEC-ID: SPEC-DEVICE-001
Title: RX 장치 기능정지 상태 감지 및 자동 복구
Created: 2026-01-19
Status: Planned
Priority: Medium
Assigned: Alfred
Traceability:
  - Module: components/03_service/device_manager/device_manager.cpp
  - Module: components/00_common/lora_protocol/include/lora_protocol.h
  - Module: components/00_common/event_bus/include/event_bus.h
```

---

## 환경 (Environment)

### 시스템 환경
- **하드웨어**: ESP32-S3 (EoRa-S3)
- **펌웨어**: ESP-IDF 5.5.0 기반 TX/RX 모드
- **LoRa 프로토콜**: 커스텀 프로토콜 (상태 응답 0xD0)

### 선행 조건
- TX 모드에서 주기적으로 상태 요청 (0xE0) 송신
- RX 모드에서 상태 응답 (0xD0) 송신
- `lora_msg_status_t` 구조체에 `stopped` 필드 이미 존재 (0: 정상, 1: 기능정지)
- `device_info_t` 구조체에 `is_stopped` 필드 이미 존재

---

## 가정 (Assumptions)

1. **stopped 상태 의미**: RX 장치가 기능정지 명령(0xE4)을 받아서 디스플레이/LED가 꺼진 상태
2. **지속성 문제**: RX가 재부팅해도 stopped 상태가 유지됨 (라이선스 초과 등으로 인해)
3. **복구 필요성**: TX가 stopped 상태의 RX를 감지하고 자동으로 복구해야 함
4. **간단한 로직**: 사용자가 요청한 대로 복잡한 상태 기계 없이 단순한 조건문으로 구현

---

## 요구사항 (Requirements)

### REQ-1: 상태 응답에 기능정지 상태 포함 (이미 구현됨)
**시스템은** RX 장치의 상태 응답(0xD0)에 `stopped` 필드를 포함해야 한다.

**검증 방법**:
- `lora_msg_status_t` 구조체에 `stopped` 필드 존재
- RX가 `send_status_response()`에서 `s_status.stopped = s_rx.stopped ? 1 : 0` 설정
- TX가 `on_status_response()`에서 `is_stopped` 업데이트

### REQ-2: TX의 기능정지 상태 감지
**WHEN** TX가 RX의 상태 응답(0xD0)을 수신하고 `stopped == 1`이면, **시스템은** 해당 장치가 기능정지 상태임을 감지해야 한다.

**검증 방법**:
- `on_status_response()` 함수에서 `status->stopped == 1` 확인
- `device_info.is_stopped`가 `true`로 설정됨
- 로그에 "device in stopped state" 메시지 출력

### REQ-3: 기능정지 상태 RX 장치 복구 명령 송신
**WHEN** TX가 NVS에 등록된 RX 장치가 기능정지 상태임을 감지하면, **시스템은** 즉시 기능정지 해제 명령을 송신해야 한다.

**검증 방법**:
- `status->stopped == 1`인 장치에 대해 복구 명령 송신
- 장치 ID가 NVS에 등록되어 있는지 확인 (이미 리스트에 존재하면 등록된 것)
- 단순한 조건문으로 구현 (복잡한 상태 기계 없음)

### REQ-4: 기능정지 해제 프로토콜
**IF** 기능정지 상태의 RX가 복구 명령을 수신하면, **시스템은** 기능정지 상태를 해제하고 정상 동작을 재개해야 한다.

**검증 방법**:
- RX가 복구 명령을 수신하면 `s_rx.stopped = false` 설정
- `EVT_STOP_CHANGED` 이벤트 발행으로 디스플레이/LED 재활성화
- 즉시 상태 응답(0xD0) 송신으로 변경된 상태 TX에 통보

---

## 상세 설명 (Specifications)

### SPEC-1: 상태 응답 구조체 확인

기존 `lora_msg_status_t` 구조체에 이미 `stopped` 필드가 존재함을 확인:

```cpp
// lora_protocol.h (Line 100-110)
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xD0
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint8_t battery;                 // 0-100
    uint8_t camera_id;
    uint32_t uptime;                 // 초
    uint8_t brightness;              // 0-255
    uint16_t frequency;              // 현재 주파수 (MHz, 정수)
    uint8_t sync_word;               // 현재 sync word
    uint8_t stopped;                 // 0: 정상, 1: 기능 정지 상태
} lora_msg_status_t;
```

### SPEC-2: TX 상태 응답 수신 처리 수정

`device_manager.cpp`의 `on_status_response()` 함수에 기능정지 상태 복구 로직 추가:

```cpp
// device_manager.cpp - on_status_response() 함수 내
static void on_status_response(const lora_msg_status_t* status, int16_t rssi, float snr)
{
    // ... 기존 코드 ...

    // 기존: 기능정지 상태 저장
    s_tx.devices[found_idx].is_stopped = (status->stopped == 1);

    // 추가: 기능정지 상태 복구
    if (status->stopped == 1) {
        char device_id_str[5];
        lora_device_id_to_str(status->device_id, device_id_str);
        T_LOGI(TAG, "device in stopped state, sending recovery: ID=%s", device_id_str);

        // 기능정지 해제 명령 송신 (새로운 명령 또는 기존 메커니즘 활용)
        send_stop_release_command(status->device_id);
    }
}
```

### SPEC-3: 기능정지 해제 명령 송신 함수

두 가지 접근 방식 중 선택:

**옵션 A: 새로운 명령 (0xE9) 추가**
```cpp
#define LORA_HDR_STOP_RELEASE  0xE9   // 기능 정지 해제 (Unicast)

typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE9
    uint8_t device_id[LORA_DEVICE_ID_LEN];
} lora_cmd_stop_release_t;

static esp_err_t send_stop_release_command(const uint8_t* device_id)
{
    static lora_cmd_stop_release_t s_cmd;
    s_cmd.header = LORA_HDR_STOP_RELEASE;
    memcpy(s_cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_cmd,
        .length = sizeof(s_cmd)
    };

    return event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
}
```

**옵션 B: 기존 메커니즘 활용 (권장, 간단)**
- `stopped = 0`인 상태 응답을 주기적으로 다시 요청하여 RX가 갱신하도록 유도
- 또는 특별한 명령 없이, RX가 상태 요청(0xE0)을 받으면 자동으로 stopped 상태를 해제하도록 수정

### SPEC-4: RX 기능정지 해제 처리

**옵션 A 구현 시 (새로운 명령)**:
```cpp
// device_manager.cpp - RX 모드
static void handle_stop_release_command(const lora_cmd_stop_release_t* cmd)
{
    if (!is_my_device(cmd->device_id)) {
        return;
    }

    if (s_rx.stopped) {
        s_rx.stopped = false;
        char device_id_str[5];
        lora_device_id_to_str(cmd->device_id, device_id_str);
        T_LOGI(TAG, "stop release received: ID=%s, display/LED restored", device_id_str);

        // 기능 정지 해제 이벤트 발행
        bool stopped_val = false;
        event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));

        // 즉시 상태 응답 송신으로 변경 통보
        send_status_response();
    }
}
```

**옵션 B 구현 시 (간단, 권장)**:
```cpp
// device_manager.cpp - RX 모드, handle_status_request() 함수 수정
static void handle_status_request(const lora_packet_event_t* packet)
{
    T_LOGI(TAG, "status request received (RSSI:%d)", packet->rssi);

    // 기능정지 상태 자동 해제 (TX가 상태 요청을 보내면 복구)
    if (s_rx.stopped) {
        s_rx.stopped = false;
        T_LOGI(TAG, "auto-recovering from stopped state");

        // 기능 정지 해제 이벤트 발행
        bool stopped_val = false;
        event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));
    }

    // 충돌 방지를 위한 랜덤 지연 (0-1000ms)
    uint32_t delay_ms = esp_random() % 1000;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    send_status_response();
}
```

### SPEC-5: 권장 구현 방식 (옵션 B - 가장 간단)

**TX 측 변경**: 없음 (기존 코드 동작)
- `on_status_response()`에서 이미 `is_stopped`를 업데이트함
- 주기적인 상태 요청(0xE0)이 계속 송신됨

**RX 측 변경**: `handle_status_request()` 함수만 수정
- 상태 요청 수신 시 `stopped == true`면 자동으로 `false`로 변경
- `EVT_STOP_CHANGED` 이벤트 발행으로 디스플레이/LED 복구
- 상태 응답 송신으로 변경된 상태 통보

**장점**:
1. 새로운 프로토콜 명령 불필요
2. 코드 변경 최소화
3. 단순한 조건문으로 구현 가능
4. TX가 주기적 상태 요청을 이미 수행 중이므로 추가 오버헤드 없음

---

## 추적성 (Traceability)

### 구현 파일
- `components/03_service/device_manager/device_manager.cpp`:
  - TX: `on_status_response()` 함수 (기존 동작 확인)
  - RX: `handle_status_request()` 함수 (복구 로직 추가)
- `components/00_common/lora_protocol/include/lora_protocol.h`:
  - `lora_msg_status_t` 구조체 (stopped 필드 확인)
- `components/00_common/event_bus/include/event_bus.h`:
  - `device_info_t` 구조체 (is_stopped 필드 확인)

### 관련 이벤트
- `EVT_STOP_CHANGED`: 기능 정지 상태 변경 이벤트
- `EVT_LORA_TX_COMMAND`: TX 명령 수신 이벤트
- `EVT_DEVICE_LIST_CHANGED`: 디바이스 리스트 변경 이벤트

### 종속 SPEC
- 없음 (독립된 장치 관리 기능)

### 프로토콜 명령 참조
- `LORA_HDR_STATUS_REQ` (0xE0): 상태 요청 (Broadcast)
- `LORA_HDR_STATUS` (0xD0): 상태 응답
- `LORA_HDR_STOP` (0xE4): 기능 정지 명령
