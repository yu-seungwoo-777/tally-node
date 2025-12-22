# Network 컴포넌트

EoRa-S3 Tally System의 네트워크 관리 컴포넌트입니다.

## 개요

이 컴포넌트는 WiFi와 W5500 SPI Ethernet을 관리합니다.

### 지원 기능

- **WiFi**: STA 모드 및 AP 모드 동시 지원
- **W5500 SPI Ethernet**: 유선 이더넷 연결 (ESP-IDF 5.5.0)

## 디렉토리 구조

```
components/network/
├── core/
│   ├── WiFiCore.cpp       # WiFi 제어
│   ├── EthernetCore.cpp   # W5500 Ethernet 제어
│   └── utils.h            # 핀 정의
├── manager/
│   └── NetworkManager.cpp # 네트워크 통합 관리
├── CMakeLists.txt
└── README.md              # 이 파일
```

## W5500 SPI Ethernet 설정

### 하드웨어 연결 (EoRa-S3 기본 핀)

| 신호 | GPIO | 설명 |
|------|------|------|
| MOSI | 16   | SPI MOSI |
| MISO | 15   | SPI MISO |
| SCLK | 48   | SPI Clock |
| CS   | 47   | Chip Select |
| RST  | 12   | Reset (하드웨어 리셋) |
| INT  | -1   | Interrupt (미사용, 폴링 모드) |

### ESP-IDF 5.5.0 API 변경 사항

ESP-IDF 5.0부터 W5500 초기화 방식이 변경되었습니다:

#### 이전 방식 (ESP-IDF 4.x)

```cpp
// 사용자가 직접 spi_bus_add_device() 호출 필요
spi_device_handle_t spi_handle;
spi_bus_add_device(SPI_HOST, &devcfg, &spi_handle);

eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config);  // 1개 파라미터
```

#### 현재 방식 (ESP-IDF 5.x)

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

// 2개 파라미터 필요: (SPI_HOST, devcfg 포인터)
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

### sdkconfig 설정

W5500을 사용하려면 다음 설정이 필요합니다:

```ini
# sdkconfig.defaults에 추가
CONFIG_ETH_USE_SPI_ETHERNET=y
CONFIG_ETH_SPI_ETHERNET_W5500=y
```

### 필수 헤더 파일

```cpp
#include "esp_eth_mac_spi.h"  // W5500 MAC API
#include "esp_eth_phy.h"      // W5500 PHY API
#include "esp_eth_driver.h"   // 이더넷 드라이버
```

### 주요 차이점 요약

| 항목 | ESP-IDF 4.x | ESP-IDF 5.x |
|------|-------------|-------------|
| SPI 디바이스 생성 | 수동 (`spi_bus_add_device`) | 자동 (내부 처리) |
| `ETH_W5500_DEFAULT_CONFIG` | 1개 파라미터 (spi_handle) | 2개 파라미터 (spi_host, devcfg*) |
| `esp_eth_mac_new_w5500` | 1개 파라미터 | 2개 파라미터 (w5500_config, mac_config) |
| INT 핀 미사용 시 | 자동 폴링 | `poll_period_ms` 명시 필수 |

## 사용 예제

### EthernetCore 초기화

```cpp
#include "EthernetCore.h"

// DHCP 모드
esp_err_t ret = EthernetCore::init(true);

// Static IP 모드
esp_err_t ret = EthernetCore::init(
    false,                  // DHCP 비활성화
    "192.168.0.100",       // Static IP
    "255.255.255.0",       // Netmask
    "192.168.0.1"          // Gateway
);
```

### 상태 확인

```cpp
EthernetStatus status = EthernetCore::getStatus();

if (status.initialized) {
    printf("Ethernet 초기화됨\n");
    printf("링크: %s\n", status.link_up ? "연결됨" : "연결 안됨");
    printf("IP: %s\n", status.ip);
    printf("MAC: %s\n", status.mac);
}
```

### 런타임 IP 변경

```cpp
// Static IP로 변경
EthernetCore::enableStatic("192.168.1.100", "255.255.255.0", "192.168.1.1");

// DHCP로 변경
EthernetCore::enableDHCP();
```

## 트러블슈팅

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
   pio run -t clean
   pio run
   ```

### 런타임 에러: `invalid configuration argument combination`

**원인**: INT 핀이 -1인데 폴링 모드가 설정되지 않음

**해결**:
```cpp
if (w5500_config.int_gpio_num < 0) {
    w5500_config.poll_period_ms = 100;  // 폴링 모드 활성화
}
```

### 링크가 올라오지 않음

**확인 사항**:
1. 이더넷 케이블 연결 확인
2. 네트워크 스위치/라우터 전원 확인
3. W5500 모듈 전원 (3.3V) 확인
4. SPI 핀 연결 확인 (특히 MISO/MOSI)

**디버깅**:
```cpp
// 링크 상태 확인
bool link_up = EthernetCore::isLinkUp();
printf("Link: %s\n", link_up ? "UP" : "DOWN");

// MAC 주소 확인 (정상 초기화되면 MAC 주소가 표시됨)
EthernetStatus status = EthernetCore::getStatus();
printf("MAC: %s\n", status.mac);
```

## 참고 자료

### 공식 문서

- [ESP-IDF Ethernet API Reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_eth.html)
- [ESP-IDF 5.0 Networking Migration Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/migration-guides/release-5.x/5.0/networking.html)

### W5500 데이터시트

- [WIZnet W5500 Datasheet](https://www.wiznet.io/product-item/w5500/)
- SPI 클럭: 최대 80MHz (현재 20MHz 사용)
- 전원: 3.3V ±5%
- 하드웨어 TCP/IP 스택 내장

## 라이선스

이 컴포넌트는 ESP-IDF의 일부로 Apache License 2.0을 따릅니다.
