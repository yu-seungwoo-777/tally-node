# 아키텍처

**작성일**: 2025-12-23
**버전**: 3.2 (5계층 아키텍처 + 네트워크 동작 확인 완료)

---

## 개요

ESP32-S3 (EoRa-S3) 기반 LoRa 통신 프로젝트의 5계층 아키텍처입니다.

---

## 계층 구조

```
┌─────────────────────────────────────────────────────────┐
│ 01_app (앱)                                             │
│ - lora_test: LoRa 테스트 앱                            │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 02_presentation (프레젠테이션)                         │
│ - (비어있음)                                            │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 03_service (서비스)                                     │
│ - button_poll: 버튼 폴링 (상태 머신)                   │
│ - lora_service: LoRa 통신 서비스                       │
│ - network_service: 네트워크 통합 관리                  │
│ - config_service: NVS 설정 관리                        │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 04_driver (드라이버)                                    │
│ - lora_driver: RadioLib 래퍼                           │
│ - wifi_driver: WiFi AP+STA 제어 (C++)                  │
│ - ethernet_driver: W5500 Ethernet 제어 (C++)            │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 05_hal (하드웨어 추상화)                               │
│ - lora_hal: SX1262 하드웨어 제어                        │
│ - wifi_hal: esp_wifi 캡슐화 (C)                        │
│ - ethernet_hal: W5500 SPI/GPIO (C)                      │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 00_common (공통)                                        │
│ - event_bus: 이벤트 버스 시스템                         │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 프로젝트 전역 (include/)                                │
│ - PinConfig.h: EoRa-S3 핀 맵 정의                      │
└─────────────────────────────────────────────────────────┘
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
│ 01_app (lora_test)                                  │
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

---

## 컴포넌트 상세

### 00_common - 공통

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| event_bus | 컴포넌트 간 이벤트 기반 통신 | freertos | ✅ |

### 프로젝트 전역 (include/)

| 파일 | 역할 | 상태 |
|------|------|------|
| PinConfig.h | EoRa-S3 핀 맵 정의 | ✅ |

### 01_app - 앱

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| lora_test | LoRa 테스트 앱 | button_poll, lora_service, event_bus | ✅ |
| network_test | 네트워크 테스트 앱 (WiFi/Ethernet) | network_service, config_service | ✅ |

### 02_presentation - 프레젠테이션

(비어있음)

### 03_service - 서비스

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| button_poll | 버튼 폴링 (상태 머신) | driver, esp_timer | ✅ |
| lora_service | LoRa 통신 서비스 | lora_driver, event_bus | ✅ |
| network_service | 네트워크 통합 관리 (C++) | wifi_driver, ethernet_driver, config_service | ✅ |
| config_service | NVS 설정 관리 (C++) | nvs_flash | ✅ |

### 04_driver - 드라이버

| 컴포넌트 | 역할 | 의존성 | 상태 |
|---------|------|--------|------|
| lora_driver | RadioLib 래퍼 | lora_hal, RadioLib | ✅ |
| wifi_driver | WiFi AP+STA 제어 (C++) | wifi_hal | ✅ |
| ethernet_driver | W5500 Ethernet 제어 (C++) | ethernet_hal | ✅ |

### 05_hal - 하드웨어 추상화

| 컴포넌트 | 역할 | 언어 | 의존성 | 상태 |
|---------|------|------|--------|------|
| lora_hal | LoRa 하드웨어 제어 (SX1262) | C | driver | ✅ |
| wifi_hal | esp_wifi 캡슐화 | C | esp_wifi, esp_netif | ✅ |
| ethernet_hal | W5500 SPI/GPIO (ESP-IDF 5.5.0, 동작 확인) | C | esp_eth, spi_master | ✅ |

---

## 의존성 그래프

```
lora_test (01_app)
    │
    ├─→ button_poll (03_service)
    │       └─→ driver, esp_timer
    │
    └─→ lora_service (03_service)
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
    ├─→ ethernet_driver (04_driver/C++)
    │       └─→ ethernet_hal (05_hal/C)
    │               └─→ esp_eth, spi_master
    │
    └─→ config_service (03_service/C++)
            └─→ nvs_flash

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
| 주파수 | 923.0 MHz |
| 대역폭 | 250.0 kHz |
| 확성 계수 | 9 |
| 코딩 레이트 | 5 |
| 송신 전력 | 22 dBm |
| 동기 워드 | 0x12 |

---

## 하드웨어

| 항목 | 값 |
|------|-----|
| 보드 | EoRa-S3 (ESP32-S3) |
| LoRa 칩 | SX1262 |
| 버튼 | GPIO_NUM_0 (내장 BOOT 버튼, Active Low) |

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
| LED BOARD | 37 | 보드 내장 LED |
| LED PGM | 38 | Program LED (빨간색) |
| LED PVW | 39 | Preview LED (녹색) |
| WS2812 | 45 | RGB LED |
| BAT ADC | 1 | 배터리 전압 |
