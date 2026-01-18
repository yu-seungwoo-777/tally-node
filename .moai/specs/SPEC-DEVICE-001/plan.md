# SPEC-DEVICE-001: 구현 계획

## 마일스톤

### Priority 1: RX 측 자동 복구 로직 구현
- `handle_status_request()` 함수에 기능정지 상태 확인 추가
- `stopped == true` 시 자동으로 `false`로 변경
- `EVT_STOP_CHANGED` 이벤트 발행

### Priority 2: TX 측 로깅 강화 (선택)
- `on_status_response()` 함수에 기능정지 상태 감지 로그 추가
- `is_stopped` 상태가 변경될 때 로그 출력

### Priority 3: 테스트 및 검증
- 기능정지 상태에서 복구 확인
- 주기적 상태 요청으로 자동 복구 확인

---

## 기술 접근 방식

### 핵심 원칙: 간단한 로직 (사용자 요청)

복잡한 상태 기계나 새로운 프로토콜 명령 없이, 기존 메커니즘을 활용한 자동 복구 구현.

### 1단계: RX 상태 요청 처리 수정

**파일**: `components/03_service/device_manager/device_manager.cpp`

**변경 위치**: `handle_status_request()` 함수 (RX 모드)

**변경 내용**:
```cpp
static void handle_status_request(const lora_packet_event_t* packet)
{
    T_LOGI(TAG, "status request received (RSSI:%d)", packet->rssi);

    // === 추가 시작: 기능정지 상태 자동 복구 ===
    if (s_rx.stopped) {
        s_rx.stopped = false;
        T_LOGI(TAG, "auto-recovering from stopped state (display/LED restore)");

        // 기능 정지 해제 이벤트 발행 (디스플레이/LED 서비스가 구독)
        bool stopped_val = false;
        event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));
    }
    // === 추가 종료 ===

    // 충돌 방지를 위한 랜덤 지연 (0-1000ms)
    uint32_t delay_ms = esp_random() % 1000;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    send_status_response();
}
```

### 2단계: TX 로깅 강화 (선택)

**파일**: `components/03_service/device_manager/device_manager.cpp`

**변경 위치**: `on_status_response()` 함수 (TX 모드)

**변경 내용**:
```cpp
static void on_status_response(const lora_msg_status_t* status, int16_t rssi, float snr)
{
    // ... 기존 코드 ...

    // 기존: 기능정지 상태 저장
    s_tx.devices[found_idx].is_stopped = (status->stopped == 1);

    // === 추가 시작: 기능정지 상태 로깅 ===
    if (status->stopped == 1) {
        char device_id_str[5];
        lora_device_id_to_str(status->device_id, device_id_str);
        T_LOGI(TAG, "device in stopped state: ID=%s (will auto-recover on next status request)",
                device_id_str);
    }
    // === 추가 종료 ===

    // ... 기존 코드 ...
}
```

---

## 데이터 흐름

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          기능정지 상태 복구 시퀀스                         │
└──────────────────────────────────────────────────────────────────────────┘

1. 초기 상태 (기능정지)
   TX ──────────────────────────────────────> RX (stopped = true)
   (주기적 상태 요청 0xE0 송신 중)                (디스플레이/LED 꺼짐)


2. 상태 요청 수신 및 복구
   TX ──── 0xE0 (Status Request) ────────────> RX
                                                  ↓
                                          [handle_status_request]
                                          s_rx.stopped == true?
                                                  ↓ Yes
                                          s_rx.stopped = false
                                          EVT_STOP_CHANGED 발행
                                                  ↓
                                          Display/LED 재활성화


3. 상태 응답 송신 (복구됨)
   TX <─── 0xD0 (Status Response, stopped=0) ─── RX
   ↓
[on_status_response]
is_stopped = false (업데이트)
로그: "device recovered from stopped state"


4. 정상 동작
   TX ──── Tally Data ───────────────────────> RX
                                            (디스플레이/LED 정상 표시)
```

---

## 위험 요소 및 대응 계획

### 위험 1: 복구 명령 없이 항상 복구되는 문제
**설명**: TX가 상태 요청을 보낼 때마다 RX가 자동으로 복구되어, 의도한 기능정지가 유지되지 않을 수 있음

**확률**: 높음
**영향**: 기능정지 기능이 정상 동작하지 않음

**대응**:
- 이 설계가 의도한 동작임 (사용자 요청: "쉽게 가능한 로직")
- 기능정지가 지속적으로 유지되어야 한다면, RX가 상태 요청을 무시하거나 별도의 조건 추가 필요
- 현재 요구사항에서는 "기능정지 상태 감지 후 복구"가 목표이므로 자동 복구가 올바른 동작

### 위험 2: 복구 후 디스플레이/LED가 즉시 활성화되지 않음
**설명**: `EVT_STOP_CHANGED` 이벤트를 구독하는 서비스가 응답하지 않을 수 있음

**확률**: 낮음
**영향**: 복구 로직은 실행되지만 디스플레이/LED가 켜지지 않음

**대응**:
- 기존 `handle_stop_command()`에서 동일한 이벤트를 발행하므로 이미 테스트됨
- 디스플레이/LED 서비스는 이 이벤트를 이미 구독 중

### 위험 3: 다중 RX 장치에서 충돌
**설명**: 여러 RX가 동시에 상태 응답을 송신할 때 충돌 가능성

**확률**: 중간
**영향**: 일부 RX의 복구가 지연될 수 있음

**대응**:
- 기존에 랜덤 지연(0-1000ms) 로직이 존재하므로 충돌 방지됨
- 주기적 상태 요청이 계속 송신되므로 모든 RX가 결국 복구됨

---

## 구현 우선순위

1. **RX 복구 로직**: `handle_status_request()` 함수 수정 (핵심)
2. **TX 로깅**: `on_status_response()` 함수에 로그 추가 (선택)
3. **테스트**: 기능정지 상태에서 복구 확인

---

## 코드 변경 요약

| 파일 | 함수 | 변경 타입 | 라인 수 |
|------|------|-----------|---------|
| device_manager.cpp | handle_status_request() | 로직 추가 | 약 8줄 |
| device_manager.cpp | on_status_response() | 로그 추가 (선택) | 약 5줄 |

**총 변경 라인**: 약 8-13줄 (간단한 로직)

---

## 정의된 데이터 구조체

### 기존 구조체 (변경 없음)

**lora_msg_status_t** (lora_protocol.h):
```cpp
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

**device_info_t** (event_bus.h):
```cpp
typedef struct __attribute__((packed)) {
    uint8_t device_id[2];
    int16_t last_rssi;
    int8_t last_snr;
    uint8_t battery;
    uint8_t camera_id;
    uint32_t uptime;
    uint8_t brightness;
    bool is_stopped;          ///< 기능 정지 상태 (TX 저장용)
    bool is_online;
    uint32_t last_seen;
    uint16_t ping_ms;
    float frequency;
    uint8_t sync_word;
} device_info_t;
```
