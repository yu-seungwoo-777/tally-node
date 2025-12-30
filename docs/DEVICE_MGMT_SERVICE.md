# Device Management Service 분석

**작성일**: 2025-12-31
**상태**: 리팩토링 예정

---

## 개요

`device_management_service`는 LoRa 통신을 통한 디바이스 관리 서비스로, TX(송신기)와 RX(수신기) 모드에서 서로 다른 역할을 수행합니다.

---

## 현재 구조

### 파일 위치
```
components/03_service/device_management_service/
├── device_management_service.cpp
└── include/device_management_service.h
```

### 상태 변수

#### 공통
```cpp
static bool s_initialized = false;
static bool s_started = false;
```

#### RX 전용 (28-34행)
```cpp
static bool s_stopped = false;
static uint8_t s_device_id[LORA_DEVICE_ID_LEN] = {0};
static device_mgmt_status_callback_t s_status_cb = nullptr;
static float s_rf_frequency = 0.0f;
static uint8_t s_rf_sync_word = 0;
```

#### TX 전용 (37-55행)
```cpp
// 디바이스 관리
static device_mgmt_device_t s_devices[DEVICE_MGMT_MAX_DEVICES];  // 20개
static uint8_t s_device_count = 0;
static uint8_t s_registered_devices[DEVICE_MGMT_MAX_REGISTERED][LORA_DEVICE_ID_LEN];  // 20개
static uint8_t s_registered_count = 0;
static device_mgmt_event_callback_t s_event_callback = nullptr;

// RF broadcast 태스크
static TaskHandle_t s_rf_change_task = nullptr;
static volatile bool s_rf_changing = false;
static float s_rf_new_frequency = 0.0f;
static uint8_t s_rf_new_sync_word = 0;
static lora_rf_event_t s_rf_event = {};  // @issue static 이벤트 구조체

// 디바이스 리스트 발행 태스크
static TaskHandle_t s_device_list_task = nullptr;
static volatile bool s_device_list_running = false;
static device_list_event_t s_device_list_event = {};  // @issue static 이벤트 구조체
```

---

## TX 모드 기능

### 1. 명령 송신 API

| 함수 | 설명 |
|------|------|
| `device_mgmt_send_status_req()` | 상태 요청 broadcast |
| `device_mgmt_set_brightness(device_id, brightness)` | 밝기 설정 |
| `device_mgmt_set_camera_id(device_id, camera_id)` | 카메라 ID 설정 |
| `device_mgmt_set_rf(device_id, frequency, sync_word)` | RF 설정 (broadcast 가능) |
| `device_mgmt_send_stop(device_id)` | 정지 명령 |
| `device_mgmt_reboot(device_id)` | 재부팅 |
| `device_mgmt_ping(device_id, timestamp)` | RTT 측정 |

### 2. 디바이스 목록 관리

| 함수 | 설명 |
|------|------|
| `device_mgmt_get_device_count()` | 온라인 디바이스 수 |
| `device_mgmt_get_devices(devices)` | 전체 목록 가져오기 |
| `device_mgmt_find_device(device_id)` | 디바이스 찾기 |
| `device_mgmt_get_device_at(index, device)` | 인덱스로 조회 |
| `device_mgmt_cleanup_offline(timeout_ms)` | 오프라인 디바이스 제거 |

### 3. 등록된 디바이스 관리

| 함수 | 설명 |
|------|------|
| `device_mgmt_register_device(device_id)` | 디바이스 등록 |
| `device_mgmt_unregister_device(device_id)` | 등록 해제 |
| `device_mgmt_is_registered(device_id)` | 등록 여부 확인 |
| `device_mgmt_get_registered_count()` | 등록된 수 |
| `device_mgmt_get_registered_devices(device_ids)` | 등록된 목록 |
| `device_mgmt_load_registered()` | NVS에서 로드 (event_bus 기반) |
| `device_mgmt_save_registered()` | NVS에 저장 (event_bus 기반) |
| `device_mgmt_clear_registered()` | 전체 해제 |

### 4. RF Broadcast

```cpp
// rf_change_task (68-106행)
// - 5초 동안 10회 SET_RF broadcast
// - 완료 후 EVT_RF_CHANGED 발행
```

### 5. 디바이스 리스트 발행

```cpp
// device_list_task (197-208행)
// - 5초마다 publish_device_list() 호출
// - EVT_DEVICE_LIST_CHANGED 발행
```

---

## RX 모드 기능

### 1. Device ID 관리

| 함수 | 설명 |
|------|------|
| `device_mgmt_set_device_id(device_id)` | Device ID 설정 |
| `device_mgmt_get_device_id()` | Device ID 조회 |

### 2. 명령 처리 (on_lora_packet_received)

| 명령 | 처리 |
|------|------|
| `STATUS_REQ` | `send_status()` - 상태 전송 |
| `SET_BRIGHTNESS` | `EVT_BRIGHTNESS_CHANGED` 발행 + ACK |
| `SET_CAMERA_ID` | `EVT_CAMERA_ID_CHANGED` 발행 + ACK |
| `SET_RF` | `EVT_RF_CHANGED` 발행 + ACK |
| `STOP` | `EVT_STOP_CHANGED` 발행 + ACK |
| `REBOOT` | `esp_restart()` |
| `PING` | `send_pong()` 응답 |

### 3. 응답 전송

| 함수 | 설명 |
|------|------|
| `send_ack(cmd_header, result)` | ACK 전송 |
| `send_status()` | 상태 정보 전송 |
| `send_pong(tx_timestamp_low)` | PONG 응답 |

---

## LoRa 패킷 구조

### TX → RX (명령)

| 명령 | 헤더 | 구조체 |
|------|------|--------|
| STATUS_REQ | `0x01` | `lora_cmd_header_t` |
| SET_BRIGHTNESS | `0x11` | `lora_cmd_brightness_t` |
| SET_CAMERA_ID | `0x12` | `lora_cmd_camera_id_t` |
| SET_RF | `0x21` | `lora_cmd_rf_t` |
| STOP | `0x30` | `lora_cmd_stop_t` |
| REBOOT | `0x31` | `lora_cmd_reboot_t` |
| PING | `0x40` | `lora_cmd_ping_t` |

### RX → TX (응답)

| 명령 | 헤더 | 구조체 |
|------|------|--------|
| STATUS | `0x81` | `lora_msg_status_t` |
| ACK | `0x90` | `lora_msg_ack_t` |
| PONG | `0x41` | `lora_msg_pong_t` |

---

## 이벤트 버스 사용

### 발행 (Publish)

| 이벤트 | 데이터 | 용도 |
|--------|--------|------|
| `EVT_LORA_SEND_REQUEST` | `lora_send_request_t` | LoRa 송신 요청 |
| `EVT_RF_CHANGED` | `lora_rf_event_t` | RF 설정 변경 (RX→전체) |
| `EVT_DEVICE_LIST_CHANGED` | `device_list_event_t` | 디바이스 목록 변경 |

### 구독 (Subscribe)

| 이벤트 | 핸들러 | 용도 |
|--------|---------|------|
| `EVT_LORA_PACKET_RECEIVED` | `on_lora_packet_received` | LoRa 패킷 수신 |
| `EVT_LORA_RF_BROADCAST_REQUEST` | `on_lora_rf_broadcast_request` | RF broadcast 요청 (TX) |

---

## 문제점

### 1. static 이벤트 구조체 (3개)

| 위치 | 구조체 | 용도 |
|------|--------|------|
| 49행 (TX) | `s_rf_event` | rf_change_task에서 EVT_RF_CHANGED 발행 |
| 54행 (TX) | `s_device_list_event` | publish_device_list()에서 발행 |
| 612행 (RX) | `s_rf_event` | SET_RF 수신 시 EVT_RF_CHANGED 발행 |

### 2. 단일 책임 원칙 위반

한 서비스가 너무 많은 역할을 담당:
- TX: 명령 송신 + 디바이스 관리 + 등록 관리 + RF broadcast + 리스트 발행
- RX: 명령 처리 + Device ID 관리

### 3. TX/RX 모드 의존성

`#ifdef DEVICE_MODE_TX/RX`로 분리되어 있어 유지보수가 어렵습니다.

---

## 리팩토링 계획

### 1. 서비스 분리

```
device_management_service (제거)
├── lora_tx_command_service    (TX: 명령 송신 API)
├── lora_rx_command_service    (RX: 명령 처리)
├── device_registry_service    (TX: 등록된 디바이스 관리)
└── device_list_service        (TX: 온라인 디바이스 목록)
```

### 2. 각 서비스의 역할

#### lora_tx_command_service (신설)
- TX 명령 송신 API 제공
- `device_mgmt_send_status_req()`
- `device_mgmt_set_brightness()`
- `device_mgmt_set_camera_id()`
- `device_mgmt_set_rf()`
- `device_mgmt_send_stop()`
- `device_mgmt_reboot()`
- `device_mgmt_ping()`

#### lora_rx_command_service (신설)
- RX 명령 처리
- 각 명령별 핸들러
- ACK/STATUS/PONG 전송

#### device_registry_service (신설)
- 등록된 디바이스 관리 (최대 20개)
- NVS 저장/로드는 ConfigService가 담당
- event_bus로 등록/해제 통신

#### device_list_service (신설)
- 온라인 디바이스 목록 추적 (최대 20개)
- 5초마다 EVT_DEVICE_LIST_CHANGED 발행
- 오프라인 정리

### 3. RF Broadcast 처리

lora_service 내부 또는 별도 태스크로 이동:
- `EVT_LORA_RF_BROADCAST_REQUEST` 구독
- 10회 broadcast 전송
- 완료 후 `EVT_RF_CHANGED` 발행

### 4. static 구조체 제거

모든 이벤트 발행 시 stack 변수 사용:
```cpp
// 변경 전
static lora_rf_event_t s_rf_event = {};
s_rf_event.frequency = freq;
event_bus_publish(EVT_RF_CHANGED, &s_rf_event, sizeof(s_rf_event));

// 변경 후
lora_rf_event_t event;
event.frequency = freq;
event_bus_publish(EVT_RF_CHANGED, &event, sizeof(event));
```

---

## 의존 관계

### 현재
```
device_management_service
├── lora_protocol (패킷 구조체)
├── event_bus (이벤트 통신)
└── ConfigService (NVS 저장 - event_bus 경유)
```

### 리팩토링 후
```
lora_tx_command_service
├── lora_protocol
└── event_bus

lora_rx_command_service
├── lora_protocol
└── event_bus

device_registry_service
├── lora_protocol
├── event_bus
└── ConfigService (구독만)

device_list_service
├── lora_protocol
└── event_bus
```

---

## API 변경 계획

### 네임스페이스 변경

| 현재 | 변경 후 |
|------|--------|
| `device_mgmt_*()` | `lora_tx_cmd_*()` |
| `device_mgmt_register_device()` | `device_registry_register()` |
| `device_mgmt_get_device_count()` | `device_list_get_count()` |

### 또는 계층별 접두사

| 서비스 | 접두사 |
|--------|--------|
| lora_tx_command_service | `tx_cmd_` |
| lora_rx_command_service | `rx_cmd_` |
| device_registry_service | `registry_` |
| device_list_service | `devlist_` |

---

## 태스크 및 메모리 사용

### 현재 태스크

| 태스크 | 스택 | 우선순위 | 코어 |
|--------|------|----------|------|
| rf_broadcast | 4096 | 5 | 1 |
| device_list | 4096 | 4 | 1 |

### 리팩토링 후

| 태스크 | 스택 | 우선순위 | 코어 | 소유자 |
|--------|------|----------|------|--------|
| rf_broadcast | 4096 | 5 | 1 | lora_service 또는 tx_command |
| device_list | 4096 | 4 | 1 | device_list_service |
