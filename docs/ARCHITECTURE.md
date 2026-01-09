실제 폴더 구조와 현재 문서를 비교한 결과:

1. **03_service/tally_test_service/** - 실제 폴더에 존재하나 문서에 누락됨
2. **04_driver/switcher_driver/obs/** - 문서에 나와있으나 실제로는 존재하지 않음

문서를 수정하여 실제 폴더 구조와 일치시키겠습니다.

# 아키텍처

**작성일**: 2026-01-10
**버전**: 4.3 (계층 위반 수정: web_server → license_service, prod_rx_app → DisplayManager)

---

## 개요

ESP32-S3 (EoRa-S3) 기반 LoRa 통신 프로젝트의 5계층 아키텍처입니다.

---

## 계층 구조

```
┌─────────────────────────────────────────────────────────┐
│ 01_app (앱)                                             │
│ - prod_tx_app: 프로덕션 Tally 송신기 앱                │
│ - prod_rx_app: 프로덕션 Tally 수신기 앱                │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 02_presentation (프레젠테이션)                         │
│ - display: 디스플레이 관리                             │
│   - DisplayManager: 디스플레이 관리자                  │
│   - icons: 아이콘 리소스                               │
│   - pages: 화면 페이지 (Boot, Rx, Tx)                  │
│ - web_server: 웹 서버                                  │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 03_service (서비스)                                     │
│ - button_service: 버튼 서비스                          │
│ - config_service: NVS 설정 관리                        │
│ - device_manager: 디바이스 관리                        │
│ - hardware_service: 하드웨어 모니터링                   │
│ - led_service: LED 서비스                              │
│ - license_service: 라이선스 서비스                     │
│ - lora_service: LoRa 통신 서비스                       │
│ - network_service: 네트워크 통합 관리                  │
│ - switcher_service: 스위처 연결 서비스                 │
│ - tally_test_service: Tally 테스트 서비스              │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 04_driver (드라이버)                                    │
│ - battery_driver: 배터리 드라이버                      │
│ - board_led_driver: 보드 LED 드라이버                  │
│ - display_driver: 디스플레이 드라이버                  │
│ - ethernet_driver: W5500 Ethernet 제어                 │
│ - license_client: 라이선스 클라이언트 드라이버         │
│ - lora_driver: RadioLib 래퍼                           │
│ - switcher_driver: 스위처 프로토콜 드라이버            │
│   └─ atem: Blackmagic ATEM 프로토콜                   │
│   └─ vmix: vMix TCP 프로토콜                          │
│ - temperature_driver: 온도 센서 드라이버               │
│ - wifi_driver: WiFi AP+STA 제어                        │
│ - ws2812_driver: WS2812 RGB LED 드라이버              │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 05_hal (하드웨어 추상화)                               │
│ - battery_hal: 배터리 HAL                              │
│ - display_hal: 디스플레이 HAL                          │
│   └─ u8g2_hal: U8g2 래퍼                               │
│   └─ u8g2_lib: U8g2 라이브러리                         │
│ - ethernet_hal: W5500 SPI/GPIO                         │
│ - lora_hal: SX1262 하드웨어 제어                        │
│ - temperature_hal: 온도 센서 HAL                       │
│ - wifi_hal: esp_wifi 캡슐화                            │
│ - ws2812_hal: WS2812 HAL                               │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 00_common (공통)                                        │
│ - app_types: 앱 타입 정의                               │
│ - event_bus: 이벤트 버스 시스템                         │
│ - lora_protocol: LoRa 프로토콜 정의                    │
│ - t_log: 로그 유틸리티                                  │
│ - tally_types: Tally 타입 정의                          │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 프로젝트 전역 (include/)                                │
│ - PinConfig.h: EoRa-S3 핀 맵 정의                      │
└─────────────────────────────────────────────────────────┘
```

---

## 컴포넌트 폴더 구조

```
components/
├── 00_common/
│   ├── app_types/
│   ├── event_bus/
│   ├── lora_protocol/
│   ├── t_log/
│   └── tally_types/
├── 01_app/
│   ├── prod_rx_app/
│   └── prod_tx_app/
├── 02_presentation/
│   ├── display/
│   │   ├── DisplayManager/
│   │   ├── icons/
│   │   └── pages/
│   │       ├── BootPage/
│   │       ├── RxPage/
│   │       └── TxPage/
│   └── web_server/
│       └── static_embed/
├── 03_service/
│   ├── button_service/
│   ├── config_service/
│   ├── device_manager/
│   ├── hardware_service/
│   ├── led_service/
│   ├── license_service/
│   ├── lora_service/
│   ├── network_service/
│   ├── switcher_service/
│   └── tally_test_service/
├── 04_driver/
│   ├── battery_driver/
│   ├── board_led_driver/
│   ├── display_driver/
│   ├── ethernet_driver/
│   ├── license_client/
│   ├── lora_driver/
│   ├── switcher_driver/
│   │   ├── atem/
│   │   └── vmix/
│   ├── temperature_driver/
│   ├── wifi_driver/
│   └── ws2812_driver/
└── 05_hal/
    ├── battery_hal/
    ├── display_hal/
    │   ├── u8g2_hal/
    │   └── u8g2_lib/
    ├── ethernet_hal/
    ├── lora_hal/
    ├── temperature_hal/
    ├── wifi_hal/
    └── ws2812_hal/
```

---

## 의존성 규칙

### 1. 인접한 계층만 의존 (레이어 건너뛰기 금지)

하위 계층은 반드시 **인접한 상위 계층**을 통해서만 접근해야 합니다.

```
O 올바른 예:
01_app → 03_service
03_service → 04_driver
04_driver → 05_hal

X 잘못된 예 (건너뛰기):
01_app → 04_driver  (03_service를 거쳐야 함)
01_app → 05_hal     (03, 04를 거쳐야 함)
03_service → 00_common  (04, 05를 거쳐야 함)
```

### 2. event_bus로 느슨한 결합 (Loose Coupling)

**event_bus 역할**: 컴포넌트 간 직접 의존을 피하고 느슨한 결합을 제공

```
┌─────────────────────────────────────────────────────┐
│ 01_app (prod_tx_app)                                │
│   - event_bus 구독 (subscribe)                      │
│   - lora_service 이벤트 수신                         │
└─────────────────────────────────────────────────────┘
                       ▲
                       │ event
                       │
┌─────────────────────────────────────────────────────┐
│ 03_service (lora_service)                           │
│   - event_bus에 이벤트 발행 (publish)               │
│   - 01_app과 직접 의존하지 않음                      │
└─────────────────────────────────────────────────────┘
```

**사용 예시:**
- 하위 계층: `event_bus_publish(EVT_LORA_PACKET_RECEIVED, &data, size);`
- 상위 계층: `event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, callback);`

### 3. 서비스 레이어 규칙

#### 3.1 서비스 간 직접 호출 금지

같은 계층(03_service) 간 직접 호출은 금지합니다.

```
X 잘못된 예:
service_a → service_b (직접 호출)
config_service 이외의 서비스 → nvs_flash (직접 NVS 접근)

O 올바른 예:
01_app → service (App이 중개)
서비스 → event_bus → 서비스 (이벤트 기반 통신)
```

#### 3.2 NVS 접근 규칙

모든 NVS(Non-Volatile Storage) 접근은 **ConfigService만** 수행해야 합니다.

```
X 잘못된 예:
device_manager → nvs_flash (직접 NVS 접근)
기타 서비스 → nvs_flash

O 올바른 예:
서비스 → ConfigService API → NVS
또는
서비스 → event_bus → ConfigService → NVS
```

**ConfigService가 관리하는 데이터:**
- WiFi AP/STA 설정
- Ethernet 설정
- Device 설정 (brightness, camera_id, RF)
- Switcher 설정 (Primary/Secondary)
- LED 색상 설정

#### 3.3 통신 방식 가이드

| 통신 방향 | 방식 | 예시 |
|-----------|------|------|
| App → Service | 직접 호출 (생성/시작만) | `switcher_service_create()`, `network_service_init()` |
| App → Service | ❌ 제거: 설정 중계 | (이전: App이 ConfigService에서 설정을 읽어 전달) |
| Service → App | event_bus | `EVT_LORA_PACKET_RECEIVED` |
| Service → Service | event_bus | `EVT_CONFIG_DATA_CHANGED` |
| Service → Driver | 직접 호출 (하위 의존 허용) | `lora_service_send()` |
| Service → NVS | event_bus → ConfigService | `EVT_CONFIG_CHANGED` |

**v4.2 변경사항:**
- App이 ConfigService에서 설정을 읽어 다른 서비스에 전달하는 중계 역할 제거
- ConfigService 초기화 완료 시 `EVT_CONFIG_DATA_CHANGED` 발행
- 각 서비스가 이벤트를 구독하여 자체 설정 업데이트
- SwitcherService, NetworkService, LoRaService가 이벤트 기반으로 초기화

---

## 컴포넌트 상세

### 00_common - 공통

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| app_types | 앱 타입 정의 (C++) | - | ✅ |
| event_bus | 컴포넌트 간 이벤트 기반 통신 | freertos | ✅ |
| t_log | 로그 유틸리티 | - | ✅ |
| tally_types | Tally 상태 타입 정의 (C++) | - | ✅ |
| lora_protocol | LoRa 프로토콜 정의 | - | ✅ |

### 프로젝트 전역 (include/)

| 파일 | 역할 | 상태 |
|------|------|------|
| PinConfig.h | EoRa-S3 핀 맵 정의 | ✅ |

### 01_app - 앱

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| prod_tx_app | 프로덕션 Tally 송신기 앱 | switcher_service, lora_service, web_server | ✅ |
| prod_rx_app | 프로덕션 Tally 수신기 앱 | lora_service, DisplayManager | ✅ |

### 02_presentation - 프레젠테이션

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| display/DisplayManager | 디스플레이 관리자, 페이지 전환 | display_driver | ✅ |
| display/icons | 아이콘 리소스 (XBM) | - | ✅ |
| display/pages/BootPage | 부팅 화면 페이지 | DisplayManager | ✅ |
| display/pages/RxPage | RX 모드 페이지 | DisplayManager | ✅ |
| display/pages/TxPage | TX 모드 페이지 | DisplayManager | ✅ |
| web_server | 웹 설정 서버 (license_service 사용) | esp_http_server, license_service | ✅ |
| web_server/static_embed | 웹 정적 리소스 (임베디드) | - | ✅ |

### 03_service - 서비스

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| button_service | 버튼 서비스 (폴링 + 상태 머신) | esp_timer, event_bus | ✅ |
| config_service | NVS 설정 관리 (C++) | nvs_flash, event_bus | ✅ |
| device_manager | 디바이스 관리 (TX/RX 통합) | lora_service, lora_protocol, event_bus | ✅ |
| hardware_service | 하드웨어 모니터링 (배터리, 온도, RSSI/SNR) | battery_driver, temperature_driver, event_bus | ✅ |
| led_service | WS2812 LED 서비스 | ws2812_driver, board_led_driver | ✅ |
| license_service | 라이선스 인증 서비스 (C++) | license_client, event_bus | ✅ |
| lora_service | LoRa 통신 서비스 | lora_driver, event_bus | ✅ |
| network_service | 네트워크 통합 관리 (C++) | wifi_driver, ethernet_driver, event_bus | ✅ |
| switcher_service | 스위처 연결 서비스 (C++) | switcher_driver, event_bus, tally_types | ✅ |
| tally_test_service | Tally 테스트 서비스 | event_bus | ✅ |

### 04_driver - 드라이버

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| lora_driver | RadioLib 래퍼 | lora_hal, RadioLib | ✅ |
| wifi_driver | WiFi AP+STA 제어 (C++) | wifi_hal | ✅ |
| ethernet_driver | W5500 Ethernet 제어 (C++) | ethernet_hal | ✅ |
| switcher_driver/atem | Blackmagic ATEM 프로토콜 (C++) | esp_netif | ✅ |
| switcher_driver/vmix | vMix TCP 프로토콜 (C++) | esp_netif | ✅ |
| display_driver | 디스플레이 드라이버 (C++) | display_hal | ✅ |
| battery_driver | 배터리 드라이버 (C++) | battery_hal, adc | ✅ |
| board_led_driver | 보드 LED 드라이버 (C++) | gpio | ✅ |
| temperature_driver | 온도 센서 드라이버 (C++) | temperature_hal, adc | ✅ |
| ws2812_driver | WS2812 RGB LED 드라이버 (C++) | ws2812_hal, rmt | ✅ |
| license_client | 라이선스 클라이언트 드라이버 (C++) | esp_http_client | ✅ |

### 05_hal - 하드웨어 추상화

| 컴포넌트 | 역할 | 언어 | 의존성 | 상태 |
|---------|------|------|--------|------|
| lora_hal | LoRa 하드웨어 제어 (SX1262) | C | driver | ✅ |
| wifi_hal | esp_wifi 캡슐화 | C | esp_wifi, esp_netif | ✅ |
| ethernet_hal | W5500 SPI/GPIO (ESP-IDF 5.5.0, 동작 확인) | C | esp_eth, spi_master | ✅ |
| display_hal | 디스플레이 HAL (SSD1306 + U8g2) | C | gpio, i2c, spi_master | ✅ |
| display_hal/u8g2_hal | U8g2 래퍼 | C | u8g2_lib | ✅ |
| display_hal/u8g2_lib | U8g2 라이브러리 | C | - | ✅ |
| battery_hal | 배터리 HAL (ADC) | C | adc | ✅ |
| temperature_hal | 온도 센서 HAL (ADC) | C | adc | ✅ |
| ws2812_hal | WS2812 HAL (RMT) | C | rmt, gpio | ✅ |

---

## 의존성 그래프

```
# LoRa
lora_service (03_service)
    │
    └─→ lora_driver (04_driver)
            └─→ lora_hal (05_hal)
                    └─→ driver
    └─→ event_bus (00_common)

# Network (WiFi/Ethernet)
network_service (03_service)
    │
    ├─→ wifi_driver (04_driver/C++)
    │       └─→ wifi_hal (05_hal/C)
    │               └─→ esp_wifi, esp_netif
    │
    └─→ ethernet_driver (04_driver/C++)
            └─→ ethernet_hal (05_hal/C)
                    └─→ esp_eth, spi_master
    │
    └─→ event_bus (EVT_CONFIG_DATA_CHANGED, EVT_NETWORK_RESTART_REQUEST)

# Display
display_driver (04_driver/C++)
    └─→ display_hal (05_hal/C)
            ├─→ u8g2_hal
            │       └─→ u8g2_lib
            └─→ gpio, i2c, spi_master

display/DisplayManager (02_presentation/C++)
    └─→ display_driver (04_driver/C++)
            └─→ display_hal (05_hal/C)
                    ├─→ u8g2_hal
                    │       └─→ u8g2_lib
                    └─→ gpio, i2c, spi_master

# LED
led_service (03_service)
    └─→ ws2812_driver (04_driver/C++)
            └─→ ws2812_hal (05_hal/C)
                    └─→ rmt, gpio
    └─→ board_led_driver (04_driver/C++)
            └─→ gpio

# Battery
battery_driver (04_driver/C++)
    └─→ battery_hal (05_hal/C)
            └─→ adc

# Temperature
temperature_driver (04_driver/C++)
    └─→ temperature_hal (05_hal/C)
            └─→ adc

# Tally System
prod_tx_app (01_app)
    │
    ├─→ switcher_service (03_service)
    │       ├─→ switcher_driver/atem (04_driver)
    │       └─→ switcher_driver/vmix (04_driver)
    │
    └─→ lora_service (03_service)
            └─→ lora_driver (04_driver)

prod_rx_app (01_app)
    │
    ├─→ lora_service (03_service)
    │       └─→ lora_driver (04_driver)
    │
    └─→ DisplayManager (02_presentation)
            └─→ display_driver (04_driver)

# 공통
include/PinConfig.h → 모든 계층에서 사용
```

### 네트워크 기능

| 기능 | WiFi | Ethernet |
|------|------|----------|
| AP 모드 | ✅ | - |
| STA 모드 | ✅ | - |
| 스캔 | ✅ (동기) | - |
| W5500 SPI | - | ✅ |
| DHCP | ✅ | ✅ |
| Static IP | ✅ | ✅ |

---

## 버튼 동작

| 액션 | 시간 | 동작 |
|------|------|------|
| 단일 클릭 | < 1000ms | "Test" 메시지 송신 |
| 롱 프레스 | ≥ 1000ms | 통계 출력 (송신/수신/RSSI/SNR) |

---

## LoRa 설정

| 파라미터 | 값 |
|---------|-----|
| 주파수 | 868.0 MHz |
| 대역폭 | 125.0 kHz |
| 확성 계수 | 7 |
| 코딩 레이트 | 5 |
| 송신 전력 | 22 dBm |
| 동기 워드 | 0x12 |

---

## 디스플레이 설정

| 항목 | 값 |
|------|-----|
| 칩셋 | SSD1306 |
| 해상도 | 128 x 64 픽셀 |
| 통신 | I2C (800kHz) |
| 라이브러리 | U8g2 |
| 폰트 | profont11_mf (11px), profont22_mf (22px) |

### 디스플레이 좌표계

- **원점**: (0, 0) = 왼쪽 상단
- **텍스트 y 좌표**: 베이스라인 기준 (글자 하단 기준선)
- **권장 x 좌표**: ≥ 4 (테두리와 겹침 방지)

### 페이지 구조

| 페이지 | 용도 | 내용 |
|--------|------|------|
| BootPage | 부팅 화면 | 로고, 초기화 메시지 |
| RxPage | RX 모드 페이지 | 채널 번호, PVW/PGM 상태, 프로그램 이름 |
| TxPage | TX 모드 페이지 | 송신 상태, 연결 정보 |

---

## 하드웨어

| 항목 | 값 |
|------|-----|
| 보드 | EoRa-S3 (ESP32-S3) |
| LoRa 칩 | SX1262 |
| 버튼 | GPIO_NUM_0 (내장 BOOT 버튼, Active Low) |
| 디스플레이 | SSD1306 OLED (I2C) |

### 핀 맵 (include/PinConfig.h)

| 기능 | GPIO | 설명 |
|------|------|------|
| LoRa MISO | 3 | SPI2_HOST |
| LoRa MOSI | 6 | SPI2_HOST |
| LoRa SCK | 5 | SPI2_HOST |
| LoRa CS | 7 | Chip Select |
| LoRa DIO1 | 33 | IRQ |
| LoRa BUSY | 34 | Busy |
| LoRa RST | 8 | Reset |
| W5500 MISO | 15 | SPI3_HOST |
| W5500 MOSI | 16 | SPI3_HOST |
| W5500 SCK | 48 | SPI3_HOST |
| W5500 CS | 47 | Chip Select |
| W5500 RST | 12 | Reset |
| Display SDA | 43 | I2C SDA |
| Display SCL | 44 | I2C SCL |
| LED BOARD | 37 | 보드 내장 LED |
| LED PGM | 38 | Program LED (빨간색) |
| LED PVW | 39 | Preview LED (녹색) |
| WS2812 | 45 | RGB LED |
| BAT ADC | 1 | 배터리 전압 |
