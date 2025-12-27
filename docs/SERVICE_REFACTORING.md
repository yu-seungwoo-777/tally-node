# 서비스 레이어 리팩토링 계획

**작성일**: 2025-12-28
**목표**: 서비스 간 직접 호출 제거, NVS 접근 중앙화

---

## 문제 현황

| 위반 유형 | 서비스 | 대상 | 위치 |
|-----------|--------|------|------|
| 서비스 간 직접 호출 | `device_management_service` | `lora_service` | `send_packet()` |
| 서비스 간 직접 호출 | `led_service` | `config_service` | `load_colors_from_nvs()` |
| 서비스 간 직접 호출 | `network_service` | `config_service` | `init()` |
| NVS 직접 접근 | `device_management_service` | `nvs_flash` | 등록된 디바이스 관리 |

---

## 리팩토링 규칙

1. **서비스 간 직접 호출 금지**: Service → Service는 event_bus로 통신
2. **NVS는 ConfigService 전담**: 모든 NVS 접근은 ConfigService를 통해야 함
3. **App이 중개**: 필요한 데이터는 App에서 서비스로 전달

---

## Phase 1: device_management_service → lora_service (event_bus)

### 목표
LoRa 송신 요청을 event_bus를 통해 발행하도록 변경

### 작업

#### 1.1 event_bus 이벤트 정의

**파일**: `00_common/event_bus/include/event_bus.h`

```cpp
// LoRa 송신 요청 이벤트
typedef struct {
    const uint8_t* data;
    size_t length;
} lora_send_request_t;

extern event_type_t EVT_LORA_SEND_REQUEST;
```

#### 1.2 lora_service에서 이벤트 구독

**파일**: `03_service/lora_service/lora_service.cpp`

```cpp
static esp_err_t on_lora_send_request(const event_data_t* event) {
    auto* req = (const lora_send_request_t*)event->data;
    return lora_service_send(req->data, req->length);
}

// start()에서 구독
event_bus_subscribe(EVT_LORA_SEND_REQUEST, on_lora_send_request);
```

#### 1.3 device_management_service에서 이벤트 발행

**파일**: `03_service/device_management_service/device_management_service.cpp`

```cpp
// 기존
// return lora_service_send(data, length);

// 변경 후
lora_send_request_t req = { .data = data, .length = length };
event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
```

#### 1.4 의존성 제거

**파일**: `03_service/device_management_service/CMakeLists.txt`

```cmake
# 제거: lora_service
REQUIRES lora_protocol event_bus t_log nvs_flash esp_system
```

---

## Phase 2: device_management_service NVS → ConfigService

### 목표
등록된 디바이스 NVS 관리를 ConfigService로 이동

### 작업

#### 2.1 ConfigService API 추가

**파일**: `03_service/config_service/include/config_service.h`

```cpp
// 등록된 디바이스 관리
#define CONFIG_MAX_REGISTERED_DEVICES 20

typedef struct {
    uint8_t device_ids[CONFIG_MAX_REGISTERED_DEVICES][4];
    uint8_t count;
} config_registered_devices_t;

esp_err_t config_service_register_device(const uint8_t* device_id);
esp_err_t config_service_unregister_device(const uint8_t* device_id);
bool config_service_is_device_registered(const uint8_t* device_id);
uint8_t config_service_get_registered_devices(config_registered_devices_t* devices);
```

#### 2.2 device_management_service에서 ConfigService API 사용

**파일**: `03_service/device_management_service/device_management_service.cpp`

```cpp
// NVS 코드 제거, ConfigService API 호출
esp_err_t device_mgmt_register_device(const uint8_t* device_id) {
    return config_service_register_device(device_id);
}
```

#### 2.3 NVS 의존성 제거

**파일**: `03_service/device_management_service/CMakeLists.txt`

```cmake
# 제거: nvs_flash esp_system
REQUIRES lora_protocol event_bus t_log config_service
```

---

## Phase 3: led_service → config_service 제거

### 목표
LED 색상을 App에서 전달받도록 변경

### 작업

#### 3.1 led_service API 추가

**파일**: `03_service/led_service/include/led_service.h`

```cpp
// 색상 설정 (App에서 호출)
typedef struct {
    uint8_t program_r, program_g, program_b;
    uint8_t preview_r, preview_g, preview_b;
    uint8_t off_r, off_g, off_b;
    uint8_t battery_low_r, battery_low_g, battery_low_b;
} led_colors_t;

esp_err_t led_service_set_colors(const led_colors_t* colors);
esp_err_t led_service_init_with_colors(int gpio, int num_leds, uint8_t camera_id, const led_colors_t* colors);
```

#### 3.2 prod_rx_app에서 색상 전달

**파일**: `01_app/prod_rx_app/prod_rx_app.cpp`

```cpp
// init() 후
led_colors_t colors;
config_service_get_led_colors(&colors);  // App에서 config 조회
led_service_set_colors(&colors);          // led_service에 전달
```

#### 3.3 config_service 의존성 제거

**파일**: `03_service/led_service/CMakeLists.txt`

```cmake
# 제거: config_service
REQUIRES ws2812_driver t_log
```

---

## Phase 4: network_service → config_service 제거

### 목표
네트워크 설정을 App에서 전달받도록 변경

### 작업

#### 4.1 network_service API 변경

**파일**: `03_service/network_service/include/network_service.h`

```cpp
// 설정을 App에서 전달받도록 변경
typedef struct {
    config_wifi_ap_t wifi_ap;
    config_wifi_sta_t wifi_sta;
    config_ethernet_t ethernet;
} network_config_t;

esp_err_t network_service_init_with_config(const network_config_t* config);
```

#### 4.2 prod_tx_app에서 설정 전달

**파일**: `01_app/prod_tx_app/prod_tx_app.cpp`

```cpp
// 기존: network_service_init()가 내부에서 config_service 호출
// 변경: App에서 설정을 전달

network_config_t net_config;
config_service_get_wifi_ap(&net_config.wifi_ap);
config_service_get_wifi_sta(&net_config.wifi_sta);
config_service_get_ethernet(&net_config.ethernet);
network_service_init_with_config(&net_config);
```

#### 4.3 config_service 의존성 제거

**파일**: `03_service/network_service/CMakeLists.txt`

```cmake
# 제거: config_service
REQUIRES wifi_driver ethernet_driver freertos t_log
```

---

## 우선순위 및 순서

| Phase | 우선순위 | estimated | 의존 |
|-------|----------|-----------|------|
| 1. lora_service event_bus | 높음 | 중간 | - |
| 2. NVS ConfigService 이관 | 높음 | 중간 | - |
| 3. led_service | 중간 | 단순 | 2 완료 후 |
| 4. network_service | 낮음 | 단순 | - |

---

## 완료 후 검증

- [ ] 서비스 간 직접 호출 없음 (grep 검증)
- [ ] NVS 접근은 ConfigService만 (grep 검증)
- [ ] 빌드 통과 (TX/RX)
- [ ] 기능 테스트 통과
