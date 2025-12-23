# WiFi/Ethernet 리팩토링 계획

**작성일**: 2025-12-23
**기반 문서**: `docs/WIFI_ETHERNET_IMPLEMENTATION.md`
**대상**: examples/1 → 현재 프로젝트 (5계층 아키텍처)

---

## 1. 목표

examples/1의 WiFi/Ethernet 기능을 현재 프로젝트의 5계층 아키텍처로 리팩토링합니다.

**핵심 원칙:**
- examples/1의 모든 기능을 동일하게 구현
- 5계층 아키텍처 규칙 준수
- 기존 lora 기능과의 통합

---

## 2. examples/1 기능 목록

### Network 컴포넌트

| 기능 | 설명 |
|------|------|
| WiFi AP+STA | 동시 지원 |
| WiFi 스캔 | 주변 AP 스캔 (동기) |
| W5500 Ethernet | SPI 연결 (ESP-IDF 5.5.0) |
| DHCP/Static IP | 런타임 전환 |
| DHCP 폴백 | 10초 타임아웃 시 Static IP 자동 전환 |
| 상태 모니터링 | 링크, IP, RSSI 등 |
| ConfigCore 연동 | NVS 설정 로드 |

---

## 3. 5계층 아키텍처 매핑

### examples/1 → 현재 프로젝트 매핑

```
examples/1 (3계층)           →  현재 프로젝트 (5계층)
─────────────────────────────────────────────────────────
NetworkManager (Manager)    →  03_service/network_service
WiFiCore (Core)             →  04_driver/wifi_driver
EthernetCore (Core)         →  04_driver/ethernet_driver
ESP-IDF Driver              →  05_hal/wifi_hal, 05_hal/ethernet_hal
PinConfig                   →  00_common/pin_config
ConfigCore                   →  03_service/config_service
```

### 새로운 컴포넌트 구조

```
components/
├── 00_common/
│   └── pin_config/          # EoRa-S3 핀 맵 정의
│       ├── include/
│       │   └── PinConfig.h
│       └── CMakeLists.txt
│
├── 03_service/
│   ├── network_service/     # 네트워크 통합 관리 (NetworkManager)
│   │   ├── include/
│   │   │   └── NetworkService.h
│   │   ├── NetworkService.cpp
│   │   └── CMakeLists.txt
│   │
│   └── config_service/      # NVS 설정 관리 (ConfigCore)
│       ├── include/
│       │   └── ConfigService.h
│       ├── ConfigService.cpp
│       └── CMakeLists.txt
│
├── 04_driver/
│   ├── wifi_driver/         # WiFi 제어 (WiFiCore)
│   │   ├── include/
│   │   │   └── WiFiDriver.h
│   │   ├── WiFiDriver.cpp
│   │   └── CMakeLists.txt
│   │
│   └── ethernet_driver/     # W5500 Ethernet 제어 (EthernetCore)
│       ├── include/
│       │   └── EthernetDriver.h
│       ├── EthernetDriver.cpp
│       └── CMakeLists.txt
│
└── 05_hal/
    ├── wifi_hal/           # WiFi HAL (esp_wifi 캡슐화)
    │   ├── include/
    │   │   └── WiFiHal.h
    │   ├── WiFiHal.c
    │   └── CMakeLists.txt
    │
    └── ethernet_hal/       # Ethernet HAL (SPI/GPIO)
        ├── include/
        │   └── EthernetHal.h
        ├── EthernetHal.c
        └── CMakeLists.txt
```

---

## 4. 컴포넌트별 상세 설계

### 4.1 00_common/pin_config

**역할:** EoRa-S3 핀 맵 중앙 정의

```cpp
// PinConfig.h
#pragma once

// I2C (OLED)
#define EORA_S3_I2C_SDA         GPIO_NUM_18
#define EORA_S3_I2C_SCL         GPIO_NUM_17
#define EORA_S3_I2C_PORT        I2C_NUM_0

// LoRa (SX1262) - SPI2_HOST
#define EORA_S3_LORA_MISO       GPIO_NUM_3
#define EORA_S3_LORA_MOSI       GPIO_NUM_6
#define EORA_S3_LORA_SCK        GPIO_NUM_5
#define EORA_S3_LORA_CS         GPIO_NUM_7
#define EORA_S3_LORA_DIO1       GPIO_NUM_33
#define EORA_S3_LORA_BUSY       GPIO_NUM_34
#define EORA_S3_LORA_RST        GPIO_NUM_8
#define EORA_S3_LORA_SPI_HOST   SPI2_HOST

// W5500 Ethernet - SPI3_HOST
#define EORA_S3_W5500_MISO      GPIO_NUM_15
#define EORA_S3_W5500_MOSI      GPIO_NUM_16
#define EORA_S3_W5500_SCK       GPIO_NUM_48
#define EORA_S3_W5500_CS        GPIO_NUM_47
#define EORA_S3_W5500_INT       -1
#define EORA_S3_W5500_RST       GPIO_NUM_12
#define EORA_S3_W5500_SPI_HOST  SPI3_HOST

// LED
#define EORA_S3_LED_BOARD       GPIO_NUM_37
#define EORA_S3_LED_PGM         GPIO_NUM_38
#define EORA_S3_LED_PVW         GPIO_NUM_39
#define EORA_S3_LED_WS2812      GPIO_NUM_45

// 버튼
#define EORA_S3_BUTTON          GPIO_NUM_0

// ADC
#define EORA_S3_BAT_ADC         GPIO_NUM_1
```

**의존성:** driver

---

### 4.2 05_hal/wifi_hal

**역할:** ESP-IDF WiFi 하드웨어 제어 캡슐화

```c
// WiFiHal.h
#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

typedef void (*wifi_event_callback_t)(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data);

// 초기화
esp_err_t wifi_hal_init(void);

// netif 생성
esp_netif_t* wifi_hal_create_ap_netif(void);
esp_netif_t* wifi_hal_create_sta_netif(void);

// 이벤트 핸들러 등록
esp_err_t wifi_hal_register_event_handler(wifi_event_callback_t callback);

// WiFi 제어
esp_err_t wifi_hal_start(void);
esp_err_t wifi_hal_stop(void);
esp_err_t wifi_hal_connect(void);
esp_err_t wifi_hal_disconnect(void);

// 설정
esp_err_t wifi_hal_set_config(wifi_interface_t iface, const wifi_config_t* config);
esp_err_t wifi_hal_get_config(wifi_interface_t iface, wifi_config_t* config);

// 스캔
esp_err_t wifi_hal_scan_start(void);
esp_err_t wifi_hal_scan_get_results(wifi_ap_record_t* ap_records, uint16_t* count);

// 정리
esp_err_t wifi_hal_deinit(void);
```

**의존성:** driver, esp_wifi, esp_netif, esp_event

---

### 4.3 05_hal/ethernet_hal

**역할:** W5500 SPI Ethernet 하드웨어 제어

```c
// EthernetHal.h
#pragma once

#include "esp_err.h"
#include "esp_eth.h"

// W5500 상태
typedef struct {
    bool initialized;
    bool link_up;
    bool got_ip;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char mac[18];
} ethernet_hal_status_t;

// 이벤트 콜백
typedef void (*ethernet_event_callback_t)(void* arg, int32_t event_id, void* event_data);
typedef void (*ip_event_callback_t)(void* arg, int32_t event_id, void* event_data);

// 초기화 (ESP-IDF 5.5.0 방식)
esp_err_t ethernet_hal_init(void);
esp_err_t ethernet_hal_deinit(void);

// 제어
esp_err_t ethernet_hal_start(void);
esp_err_t ethernet_hal_stop(void);

// 상태 조회
esp_err_t ethernet_hal_get_status(ethernet_hal_status_t* status);
bool ethernet_hal_is_link_up(void);
bool ethernet_hal_has_ip(void);

// 이벤트 핸들러 등록
esp_err_t ethernet_hal_register_event_handler(ethernet_event_callback_t cb);
esp_err_t ethernet_hal_register_ip_handler(ip_event_callback_t cb);

// IP 설정
esp_err_t ethernet_hal_enable_dhcp(void);
esp_err_t ethernet_hal_enable_static(const char* ip, const char* netmask, const char* gateway);
```

**의존성:** driver, esp_eth, esp_netif, spi_master, gpio

**구현 포인트 (ESP-IDF 5.5.0):**
```c
// SPI 디바이스 설정
spi_device_interface_config_t devcfg = {
    .command_bits = 16,
    .address_bits = 8,
    .mode = 0,
    .clock_speed_hz = 20 * 1000 * 1000,
    .queue_size = 20,
    .spics_io_num = EORA_S3_W5500_CS
};

eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI3_HOST, &devcfg);
w5500_config.int_gpio_num = -1;
w5500_config.poll_period_ms = 100;  // 폴링 모드 필수

eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_config);
```

---

### 4.4 04_driver/wifi_driver

**역할:** WiFi 기능 제어 (AP+STA)

```cpp
// WiFiDriver.h
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi 상태
typedef struct {
    bool ap_started;
    bool sta_connected;
    char ap_ip[16];
    char sta_ip[16];
    int8_t sta_rssi;
    uint8_t ap_clients;
} wifi_status_t;

// 스캔 결과
typedef struct {
    char ssid[33];
    uint8_t channel;
    int8_t rssi;
    uint8_t auth_mode;
} wifi_scan_result_t;

// 상태 변경 콜백
typedef void (*wifi_status_callback_t)(void);

// 초기화 (AP+STA)
esp_err_t wifi_driver_init(const char* ap_ssid, const char* ap_password,
                          const char* sta_ssid, const char* sta_password);

// 정리
esp_err_t wifi_driver_deinit(void);

// 상태 조회
wifi_status_t wifi_driver_get_status(void);

// 스캔
esp_err_t wifi_driver_scan(wifi_scan_result_t* results, uint16_t max_count, uint16_t* out_count);

// STA 제어
esp_err_t wifi_driver_sta_reconnect(void);
esp_err_t wifi_driver_sta_disconnect(void);
bool wifi_driver_sta_is_connected(void);

// AP 정보
uint8_t wifi_driver_get_ap_clients(void);

// 콜백 설정
void wifi_driver_set_status_callback(wifi_status_callback_t callback);

#ifdef __cplusplus
}
#endif
```

**의존성:** wifi_hal, pin_config

---

### 4.5 04_driver/ethernet_driver

**역할:** W5500 Ethernet 제어

```cpp
// EthernetDriver.h
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 이더넷 상태
typedef struct {
    bool initialized;
    bool link_up;
    bool got_ip;
    bool dhcp_mode;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char mac[18];
} ethernet_status_t;

// 상태 변경 콜백
typedef void (*ethernet_status_callback_t)(void);

// 초기화
esp_err_t ethernet_driver_init(bool dhcp_enabled,
                              const char* static_ip,
                              const char* static_netmask,
                              const char* static_gateway);

// 정리
esp_err_t ethernet_driver_deinit(void);

// 상태 조회
ethernet_status_t ethernet_driver_get_status(void);

// IP 모드 변경
esp_err_t ethernet_driver_enable_dhcp(void);
esp_err_t ethernet_driver_enable_static(const char* ip, const char* netmask, const char* gateway);

// 제어
esp_err_t ethernet_driver_restart(void);
bool ethernet_driver_is_link_up(void);
bool ethernet_driver_has_ip(void);

// 콜백 설정
void ethernet_driver_set_status_callback(ethernet_status_callback_t callback);

#ifdef __cplusplus
}
#endif
```

**의존성:** ethernet_hal, pin_config

---

### 4.6 03_service/config_service

**역할:** NVS 설정 관리 (ConfigCore)

```cpp
// ConfigService.h
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi AP 설정
typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    bool enabled;
} config_wifi_ap_t;

// WiFi STA 설정
typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} config_wifi_sta_t;

// Ethernet 설정
typedef struct {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
    bool enabled;
} config_ethernet_t;

// 전체 설정
typedef struct {
    config_wifi_ap_t wifi_ap;
    config_wifi_sta_t wifi_sta;
    config_ethernet_t ethernet;
} config_all_t;

// 초기화
esp_err_t config_service_init(void);

// 설정 로드/저장
esp_err_t config_service_load_all(config_all_t* config);
esp_err_t config_service_save_all(const config_all_t* config);

// 개별 설정
esp_err_t config_service_get_wifi_ap(config_wifi_ap_t* config);
esp_err_t config_service_set_wifi_ap(const config_wifi_ap_t* config);
esp_err_t config_service_get_wifi_sta(config_wifi_sta_t* config);
esp_err_t config_service_set_wifi_sta(const config_wifi_sta_t* config);
esp_err_t config_service_get_ethernet(config_ethernet_t* config);
esp_err_t config_service_set_ethernet(const config_ethernet_t* config);

// 기본값 로드
esp_err_t config_service_load_defaults(void);

#ifdef __cplusplus
}
#endif
```

**의존성:** nvs_flash

---

### 4.7 03_service/network_service

**역할:** 네트워크 통합 관리 (NetworkManager)

```cpp
// NetworkService.h
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 네트워크 인터페이스 타입
typedef enum {
    NETWORK_IF_WIFI_AP = 0,
    NETWORK_IF_WIFI_STA,
    NETWORK_IF_ETHERNET,
    NETWORK_IF_MAX
} network_interface_t;

// 인터페이스 상태
typedef struct {
    bool active;
    bool connected;
    char ip[16];
    char netmask[16];
    char gateway[16];
} network_if_status_t;

// 전체 네트워크 상태
typedef struct {
    network_if_status_t wifi_ap;
    network_if_status_t wifi_sta;
    network_if_status_t ethernet;
    // 상세 정보는 driver에서 직접 조회
} network_status_t;

// 초기화 (ConfigService에서 설정 로드)
esp_err_t network_service_init(void);

// 정리
esp_err_t network_service_deinit(void);

// 상태 조회
network_status_t network_service_get_status(void);
void network_service_print_status(void);
bool network_service_is_initialized(void);

// 재시작 (설정 변경 후)
esp_err_t network_service_restart_wifi(void);
esp_err_t network_service_restart_ethernet(void);

#ifdef __cplusplus
}
#endif
```

**의존성:** wifi_driver, ethernet_driver, config_service, event_bus

---

## 5. 의존성 그래프

```
┌─────────────────────────────────────────────────────────┐
│ 01_app (앱)                                             │
│ - 테스트 앱, CLI 등                                     │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 02_presentation (프레젠테이션)                         │
│ - (비어있음)                                            │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 03_service (서비스)                                     │
│ - network_service: 통합 관리                            │
│   └─→ wifi_driver, ethernet_driver, config_service     │
│ - config_service: NVS 설정 관리                         │
│ - lora_service: LoRa 통신                              │
│ - button_poll: 버튼 폴링                               │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 04_driver (드라이버)                                    │
│ - wifi_driver: WiFi 제어                               │
│   └─→ wifi_hal, pin_config                             │
│ - ethernet_driver: Ethernet 제어                       │
│   └─→ ethernet_hal, pin_config                         │
│ - lora_driver: LoRa 제어                               │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 05_hal (하드웨어 추상화)                               │
│ - wifi_hal: esp_wifi 캡슐화                            │
│ - ethernet_hal: W5500 SPI/GPIO                          │
│ - lora_hal: SX1262 제어                                 │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 00_common (공통)                                        │
│ - pin_config: 핀 맵 정의                               │
│ - event_bus: 이벤트 버스                               │
└─────────────────────────────────────────────────────────┘
```

---

## 6. 구현 순서 (Phase)

### Phase 1: 기본 구조 (공통/핀 설정)

| 순서 | 컴포넌트 | 작업 | 상태 |
|------|---------|------|------|
| 1.1 | 00_common/pin_config | 핀 맵 정의 | ⬜ |
| 1.2 | docs/ARCHITECTURE.md | 새 컴포넌트 반영 | ⬜ |

### Phase 2: HAL 계층

| 순서 | 컴포넌트 | 작업 | 상태 |
|------|---------|------|------|
| 2.1 | 05_hal/wifi_hal | esp_wifi 캡슐화 | ⬜ |
| 2.2 | 05_hal/ethernet_hal | W5500 SPI 제어 (ESP-IDF 5.5.0) | ⬜ |
| 2.3 | 05_hal/ethernet_hal | DHCP 폴백 (10초 타임아웃) | ⬜ |

### Phase 3: Driver 계층

| 순서 | 컴포넌트 | 작업 | 상태 |
|------|---------|------|------|
| 3.1 | 04_driver/wifi_driver | AP+STA 제어 | ⬜ |
| 3.2 | 04_driver/wifi_driver | 스캔 기능 | ⬜ |
| 3.3 | 04_driver/ethernet_driver | W5500 제어 | ⬜ |
| 3.4 | 04_driver/ethernet_driver | DHCP/Static IP 전환 | ⬜ |

### Phase 4: Service 계층

| 순서 | 컴포넌트 | 작업 | 상태 |
|------|---------|------|------|
| 4.1 | 03_service/config_service | NVS 설정 관리 | ⬜ |
| 4.2 | 03_service/config_service | 기본값 로드 | ⬜ |
| 4.3 | 03_service/network_service | 통합 관리 | ⬜ |
| 4.4 | 03_service/network_service | 재시작 기능 | ⬜ |

### Phase 5: 통합 및 테스트

| 순서 | 작업 | 상태 |
|------|------|------|
| 5.1 | 빌드 테스트 | ⬜ |
| 5.2 | WiFi AP 연결 테스트 | ⬜ |
| 5.3 | WiFi STA 연결 테스트 | ⬜ |
| 5.4 | W5500 Ethernet 테스트 | ⬜ |
| 5.5 | DHCP 폴백 테스트 | ⬜ |
| 5.6 | LoRa와 통합 테스트 | ⬜ |

---

## 7. sdkconfig 설정

```ini
# sdkconfig.defaults에 추가

# W5500 Ethernet
CONFIG_ETH_USE_SPI_ETHERNET=y
CONFIG_ETH_SPI_ETHERNET_W5500=y

# WiFi
CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=32

# SPI
CONFIG_SPI_MASTER_IN_IRAM=y
CONFIG_SPI_MASTER_ISR_IN_IRAM=y
```

---

## 8. CMakeLists.txt 등록

### 최상위 CMakeLists.txt

```cmake
# 00_common
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/00_common/pin_config"
)

# 05_hal
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/05_hal/wifi_hal"
    "${CMAKE_SOURCE_DIR}/components/05_hal/ethernet_hal"
)

# 04_driver
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/04_driver/wifi_driver"
    "${CMAKE_SOURCE_DIR}/components/04_driver/ethernet_driver"
)

# 03_service
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/03_service/config_service"
    "${CMAKE_SOURCE_DIR}/components/03_service/network_service"
)
```

---

## 9. 테스트 계획

### 9.1 단위 테스트

| 모듈 | 테스트 항목 |
|------|-------------|
| wifi_hal | 초기화, netif 생성, 이벤트 핸들러 |
| ethernet_hal | SPI 통신, W5500 초기화, 링크 감지 |
| wifi_driver | AP 시작, STA 연결, 스캔 |
| ethernet_driver | DHCP, Static IP, 폴백 |
| config_service | NVS 읽기/쓰기, 기본값 |
| network_service | 전체 초기화, 상태 조회 |

### 9.2 통합 테스트

| 시나리오 | 검증 항목 |
|----------|----------|
| WiFi AP만 | AP 시작, 클라이언트 연결 |
| WiFi STA만 | AP 연결, IP 획득 |
| WiFi AP+STA | 동시 작동 |
| Ethernet만 | 링크업, DHCP |
| WiFi + Ethernet | 동시 작동, 우선순위 |
| DHCP 실패 | Static IP 폴백 |

---

## 10. 마이그레이션 체크리스트

### 코드 마이그레이션

- [ ] examples/1 WiFiCore.cpp → wifi_hal + wifi_driver
- [ ] examples/1 EthernetCore.cpp → ethernet_hal + ethernet_driver
- [ ] examples/1 NetworkManager.cpp → network_service
- [ ] examples/1 ConfigCore.cpp → config_service
- [ ] examples/1 PinConfig.h → pin_config

### 기능 확인

- [ ] WiFi AP+STA 동시 지원
- [ ] WiFi 스캔 (동기)
- [ ] W5500 SPI Ethernet
- [ ] DHCP/Static IP 런타임 전환
- [ ] DHCP 폴백 (10초)
- [ ] 상태 모니터링
- [ ] NVS 설정 저장/로드
- [ ] ConfigService 연동

---

## 11. 리스크 및 완화

| 리스크 | 완화 방안 |
|--------|----------|
| ESP-IDF 5.5.0 API 변경 | examples/1 구현 그대로 사용 |
| W5500 하드웨어 미장착 | 초기화 실패 시 계속 진행 |
| SPI 충돌 (LoRa/W5500) | SPI2_HOST, SPI3_HOST 분리 |
| 메모리 부족 | 최적화 및 필요 시 외부 RAM 사용 |

---

## 12. 참고 문서

- `docs/WIFI_ETHERNET_IMPLEMENTATION.md` - examples/1 구현 분석
- `docs/ARCHITECTURE.md` - 5계층 아키텍처 정의
- `examples/1/components/network/` - 원본 구현
