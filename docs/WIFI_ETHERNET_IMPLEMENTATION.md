# WiFi/Ethernet 구현 분석

**작성일**: 2025-12-23
**대상**: examples/1 컴포넌트
**분석 대상**: WiFi 및 W5500 Ethernet 구현

---

## 1. 아키텍처 개요

3계층 구조로 설계되었습니다:

```
┌─────────────────────────────────────────────────────────┐
│ NetworkManager (Manager)                                │
│ - Core API 통합                                          │
│ - 비즈니스 로직 (우선순위, 장애 조치)                     │
│ - 상태 관리 (Stateful)                                   │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ Core API (Core Layer)                                   │
│ - WiFiCore: WiFi AP+STA 제어                            │
│ - EthernetCore: W5500 Ethernet 제어                     │
│ - 하드웨어 추상화, 상태 최소화, 단일 책임                │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ Hardware Driver (ESP-IDF)                               │
│ - esp_wifi, esp_eth                                     │
│ - SPI, GPIO 드라이버                                     │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 디렉토리 구조

```
components/network/
├── core/
│   ├── WiFiCore.cpp       # WiFi AP+STA 제어
│   ├── WiFiCore.h
│   ├── EthernetCore.cpp   # W5500 Ethernet 제어
│   └── EthernetCore.h
├── manager/
│   ├── NetworkManager.cpp # 네트워크 통합 관리
│   └── NetworkManager.h
├── handlers/              # 이벤트 핸들러
├── CMakeLists.txt
└── README.md
```

---

## 3. WiFiCore 구현

### 3.1 기능 개요

| 기능 | 설명 |
|------|------|
| 모드 | AP + STA 동시 지원 |
| AP | 기본 핫스팟 모드 (최대 4명 연결) |
| STA | 외부 AP 연결 (최대 5회 재시도) |
| 스캔 | 주변 AP 스캔 (동기) |

### 3.2 상태 구조

```cpp
struct WiFiStatus {
    bool ap_started;        // AP 시작 여부
    bool sta_connected;     // STA 연결 여부
    char ap_ip[16];         // AP IP 주소
    char sta_ip[16];        // STA IP 주소
    int8_t sta_rssi;        // STA 신호 강도
    uint8_t ap_clients;     // AP 연결 클라이언트 수
};

struct WiFiScanResults {
    char ssid[33];          // SSID
    uint8_t channel;        // 채널
    int8_t rssi;            // 신호 강도
    wifi_auth_mode_t auth_mode;  // 인증 모드
};
```

### 3.3 주요 API

```cpp
class WiFiCore {
public:
    // 초기화 (AP+STA)
    static esp_err_t init(const char* ap_ssid, const char* ap_password,
                         const char* sta_ssid = nullptr,
                         const char* sta_password = nullptr);

    // 상태 조회
    static WiFiStatus getStatus();

    // AP 스캔 (동기)
    static esp_err_t scan(WiFiScanResults* out_results,
                         uint16_t max_results,
                         uint16_t* out_count);

    // STA 제어
    static esp_err_t reconnectSTA();
    static esp_err_t disconnectSTA();
    static bool isSTAConnected();

    // AP 정보
    static uint8_t getAPClients();

private:
    // 싱글톤 패턴
    WiFiCore() = delete;

    // 이벤트 핸들러
    static void eventHandler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data);
};
```

### 3.4 이벤트 처리

| 이벤트 | 동작 |
|--------|------|
| WIFI_EVENT_AP_START | AP 시작 |
| WIFI_EVENT_AP_STACONNECTED | 클라이언트 연결 |
| WIFI_EVENT_AP_STADISCONNECTED | 클라이언트 연결 해제 |
| WIFI_EVENT_STA_START | STA 시작, 연결 시도 |
| WIFI_EVENT_STA_DISCONNECTED | STA 재연결 (최대 5회) |
| IP_EVENT_STA_GOT_IP | STA IP 획득 |
| WIFI_EVENT_SCAN_DONE | 스캔 완료 |

---

## 4. EthernetCore 구현 (W5500)

### 4.1 하드웨어 핀 구성 (EoRa-S3)

| 신호 | GPIO | 설명 |
|------|------|------|
| MOSI | 16 | SPI MOSI |
| MISO | 15 | SPI MISO |
| SCLK | 48 | SPI Clock |
| CS | 47 | Chip Select |
| RST | 12 | 하드웨어 리셋 |
| INT | -1 | 미사용 (폴링 모드) |
| SPI_HOST | SPI3_HOST | SPI 버스 |

```cpp
// PinConfig.h
#define EORA_S3_W5500_MISO      GPIO_NUM_15
#define EORA_S3_W5500_MOSI      GPIO_NUM_16
#define EORA_S3_W5500_SCK       GPIO_NUM_48
#define EORA_S3_W5500_CS        GPIO_NUM_47
#define EORA_S3_W5500_INT       -1
#define EORA_S3_W5500_RST       GPIO_NUM_12
#define EORA_S3_W5500_SPI_HOST  SPI3_HOST
```

### 4.2 상태 구조

```cpp
struct EthernetStatus {
    bool initialized;       // 초기화 여부
    bool link_up;           // 링크 상태
    bool got_ip;            // IP 할당 여부
    bool dhcp_mode;         // DHCP 모드
    char ip[16];            // IP 주소
    char netmask[16];       // 넷마스크
    char gateway[16];       // 게이트웨이
    char mac[18];           // MAC 주소
};
```

### 4.3 주요 API

```cpp
class EthernetCore {
public:
    // 초기화 (DHCP/Static IP)
    static esp_err_t init(bool dhcp_enabled = true,
                         const char* static_ip = "192.168.0.100",
                         const char* static_netmask = "255.255.255.0",
                         const char* static_gateway = "192.168.0.1");

    // 상태 조회
    static EthernetStatus getStatus();

    // IP 모드 변경
    static esp_err_t enableDHCP();
    static esp_err_t enableStatic(const char* ip, const char* netmask,
                                 const char* gateway);

    // 제어
    static esp_err_t restart();
    static bool isLinkUp();
    static bool hasIP();

private:
    // 싱글톤 패턴
    EthernetCore() = delete;

    // 이벤트 핸들러
    static void ethEventHandler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
    static void ipEventHandler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);

    // DHCP 타임아웃 태스크
    static void dhcpTimeoutTask(void* arg);
};
```

### 4.4 ESP-IDF 5.5.0 API 변경사항

ESP-IDF 5.0부터 W5500 초기화 방식이 변경되었습니다.

#### 변경 전 (ESP-IDF 4.x)

```cpp
// 사용자가 직접 spi_bus_add_device() 호출
spi_device_handle_t spi_handle;
spi_bus_add_device(SPI_HOST, &devcfg, &spi_handle);

// 1개 파라미터
eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config);
```

#### 변경 후 (ESP-IDF 5.x)

```cpp
// SPI 버스 설정을 직접 전달 (내부적으로 spi_device 생성)
spi_device_interface_config_t devcfg = {
    .command_bits = 16,
    .address_bits = 8,
    .mode = 0,
    .clock_speed_hz = 20 * 1000 * 1000,  // 20MHz
    .queue_size = 20,
    .spics_io_num = CS_PIN
};

// 2개 파라미터 필요
eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI3_HOST, &devcfg);
w5500_config.int_gpio_num = INT_PIN;

// INT 핀 미사용 시 폴링 모드 필수
if (INT_PIN < 0) {
    w5500_config.poll_period_ms = 100;  // 100ms 폴링
}

// MAC과 PHY 생성 시 2개 파라미터 필요
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_config);
```

#### 비교표

| 항목 | ESP-IDF 4.x | ESP-IDF 5.x |
|------|-------------|-------------|
| SPI 디바이스 생성 | 수동 (`spi_bus_add_device`) | 자동 (내부 처리) |
| `ETH_W5500_DEFAULT_CONFIG` | 1개 파라미터 (spi_handle) | 2개 파라미터 (spi_host, devcfg*) |
| `esp_eth_mac_new_w5500` | 1개 파라미터 | 2개 파라미터 (w5500_config, mac_config) |
| INT 핀 미사용 시 | 자동 폴링 | `poll_period_ms` 명시 필수 |

### 4.5 하드웨어 리셋 시퀀스

```cpp
// W5500 하드웨어 리셋
gpio_set_level(EORA_S3_W5500_RST, 0);
vTaskDelay(pdMS_TO_TICKS(10));   // LOW 유지: 10ms
gpio_set_level(EORA_S3_W5500_RST, 1);
vTaskDelay(pdMS_TO_TICKS(50));   // HIGH 안정화: 50ms
```

### 4.6 DHCP 폴백 기능

- DHCP 타임아웃: **10초**
- 타임아웃 시 Static IP로 자동 전환
- 링크 재연결 시 DHCP 재시도

```cpp
void EthernetCore::dhcpTimeoutTask(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(10000));  // 10초 대기

    if (!s_got_ip && s_dhcp_mode) {
        LOG_0(TAG, "DHCP 타임아웃! Static IP로 전환...");
        enableStatic(s_static_ip, s_static_netmask, s_static_gateway);
    }

    vTaskDelete(nullptr);
}
```

---

## 5. NetworkManager (통합 관리)

### 5.1 기능 개요

| 기능 | 설명 |
|------|------|
| 초기화 | ConfigCore에서 설정 읽어 WiFi/Ethernet 초기화 |
| 상태 조회 | 전체 네트워크 상태 통합 제공 |
| 재시작 | WiFi/Ethernet 재시작 (설정 변경 시) |

### 5.2 상태 구조

```cpp
enum class NetworkInterface {
    WIFI_AP = 0,
    WIFI_STA,
    ETHERNET,
    MAX
};

struct NetworkIfStatus {
    bool active;
    bool connected;
    char ip[16];
    char netmask[16];
    char gateway[16];
};

struct NetworkStatus {
    NetworkIfStatus wifi_ap;
    NetworkIfStatus wifi_sta;
    NetworkIfStatus ethernet;
    WiFiStatus wifi_detail;      // WiFi 상세
    EthernetStatus eth_detail;    // Ethernet 상세
};
```

### 5.3 주요 API

```cpp
class NetworkManager {
public:
    // 초기화 (ConfigCore 연동)
    static esp_err_t init();

    // 전체 상태 조회
    static NetworkStatus getStatus();

    // 로그 출력
    static void printStatus();

    // 초기화 여부
    static bool isInitialized();

    // 재시작 (설정 변경 후)
    static esp_err_t restartWiFi();
    static esp_err_t restartEthernet();

private:
    // 싱글톤 패턴
    NetworkManager() = delete;
};
```

### 5.4 초기화 흐름

```
1. ConfigCore에서 설정 로드
   ├── WiFi AP 설정
   ├── WiFi STA 설정
   └── Ethernet 설정 (DHCP/Static IP)

2. WiFiCore 초기화
   └── AP + STA 모드 시작

3. EthernetCore 초기화
   └── W5500 시작 (실패 시 계속 진행)
```

---

## 6. 설계 원칙

### Core API (WiFiCore, EthernetCore)

- **하드웨어 추상화**: ESP-IDF 드라이버 캡슐화
- **상태 최소화**: 이벤트 기반 상태 관리
- **단일 책임**: 각 인터페이스 제어만 담당
- **싱글톤 패턴**: 정적 메서드만 제공

### Manager (NetworkManager)

- **Core API 통합**: WiFiCore + EthernetCore 조율
- **비즈니스 로직**: 인터페이스 우선순위, 장애 조치
- **상태 관리**: 전체 네트워크 상태 통합 관리 (Stateful)
- **ConfigCore 연동**: NVS 설정 로드 및 적용

---

## 7. sdkconfig 설정

### W5500 Ethernet 활성화

```ini
# sdkconfig.defaults에 추가
CONFIG_ETH_USE_SPI_ETHERNET=y
CONFIG_ETH_SPI_ETHERNET_W5500=y
```

### WiFi 활성화 (기본)

```ini
CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=10
CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32
CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=32
```

---

## 8. 사용 예제

### WiFiCore 사용

```cpp
#include "WiFiCore.h"

// 초기화 (AP+STA)
WiFiCore::init("Tally_AP", "password123", "HomeWiFi", "wifi_pass");

// 상태 확인
WiFiStatus status = WiFiCore::getStatus();
printf("AP IP: %s, STA 연결: %s\n",
       status.ap_ip, status.sta_connected ? "예" : "아니오");

// STA 재연결
WiFiCore::reconnectSTA();
```

### EthernetCore 사용

```cpp
#include "EthernetCore.h"

// DHCP 모드 초기화
EthernetCore::init(true);

// Static IP 모드 초기화
EthernetCore::init(false, "192.168.0.100", "255.255.255.0", "192.168.0.1");

// 상태 확인
EthernetStatus status = EthernetCore::getStatus();
printf("링크: %s, IP: %s\n",
       status.link_up ? "UP" : "DOWN", status.ip);

// 런타임에 Static IP로 변경
EthernetCore::enableStatic("192.168.1.100", "255.255.255.0", "192.168.1.1");
```

### NetworkManager 사용

```cpp
#include "NetworkManager.h"

// 초기화 (ConfigCore에서 설정 로드)
NetworkManager::init();

// 전체 상태 확인
NetworkStatus status = NetworkManager::getStatus();

// 로그 출력
NetworkManager::printStatus();
```

---

## 9. 트러블슈팅

### 컴파일 에러: `eth_w5500_config_t` was not declared

**원인**: sdkconfig에서 W5500 지원이 비활성화됨

**해결**:
1. `sdkconfig.defaults`에 다음 추가:
   ```ini
   CONFIG_ETH_USE_SPI_ETHERNET=y
   CONFIG_ETH_SPI_ETHERNET_W5500=y
   ```
2. 클린 빌드:
   ```bash
   pio run -t clean && pio run
   ```

### 런타임 에러: `invalid configuration argument combination`

**원인**: INT 핀이 -1인데 폴링 모드가 설정되지 않음

**해결**:
```cpp
if (w5500_config.int_gpio_num < 0) {
    w5500_config.poll_period_ms = 100;  // 폴링 모드 필수
}
```

### 링크가 올라오지 않음

**확인 사항**:
1. 이더넷 케이블 연결 확인
2. 네트워크 스위치/라우터 전원 확인
3. W5500 모듈 전원 (3.3V) 확인
4. SPI 핀 연결 확인 (MOSI/MOSI 교차 확인)

**디버깅**:
```cpp
bool link_up = EthernetCore::isLinkUp();
printf("Link: %s\n", link_up ? "UP" : "DOWN");

EthernetStatus status = EthernetCore::getStatus();
printf("MAC: %s\n", status.mac);  // 초기화 성공 시 MAC 주소 표시
```

---

## 10. 현재 프로젝트와의 차이점

| 항목 | examples/1 | 현재 프로젝트 |
|------|------------|---------------|
| 아키텍처 | 3계층 (Manager-Core-Driver) | 5계층 |
| WiFi | AP+STA 동시 지원 | 미구현 |
| Ethernet | W5500 SPI | 미구현 |
| Config | ConfigCore 연동 | NVS 직접 사용 |
| 로거 | simple_log | esp_log |

---

## 11. 참고 자료

- [ESP-IDF Ethernet API Reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_eth.html)
- [ESP-IDF 5.0 Networking Migration Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/migration-guides/release-5.x/5.0/networking.html)
- [WIZnet W5500 Datasheet](https://www.wiznet.io/product-item/w5500/)
