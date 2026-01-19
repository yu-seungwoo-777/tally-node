# Tally Node - 기술 스택

## 개요

Tally Node는 ESP32-S3 마이크로컨트롤러 기반의 임베디드 시스템으로, ESP-IDF 프레임워크와 PlatformIO 빌드 시스템을 사용합니다.

---

## 하드웨어 사양

### 메인 보드: EoRa-S3

| 사양 | 값 |
|------|-----|
| 제조사 | EBYTE (EoRa-S3 400TB/900TB) |
| MCU | ESP32-S3 (Xiaomi-700TB/900TB) |
| CPU 듀얼 코어 | Xtensa LX7 @ 240MHz |
| Flash | 4MB |
| PSRAM | 8MB (옵션, `BOARD_HAS_PSRAM` 정의) |
| WiFi | 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| USB CDC | 온부트 지원 (`ARDUINO_USB_CDC_ON_BOOT=1`) |
| RAM | 320KB (최대) |

### 주변 하드웨어

| 구성 요소 | 모델 | 용도 |
|-----------|------|------|
| LoRa 모듈 | SX1262 | 장거리 무선 통신 (923MHz) |
| 디스플레이 | OLED 128x64 | Tally 상태 표시 |
| LED | WS2812B | Program/Preview 표시 |
| 버튼 | 풀업 스위치 | 메뉴 네비게이션 |
| 배터리 | Li-Polymer | 포터블 운영 |

---

## 펌웨어 기술 스택

### ESP-IDF 5.5.0

**Espressif IoT Development Framework**를 사용하여 ESP32-S3의 기능을 활용합니다.

주요 구성 요소:
- **FreeRTOS**: 실시간 운영체제 (태스크 스케줄링)
- **TCP/IP Adapter**: 네트워크 스택
- **NVS (Non-Volatile Storage)**: 설정 저장소
- **ESP Timer**: 고해상도 타이머
- **RMT (Remote Control)**: WS2812 LED 제어

### ESP-IDF 5.x API 마이그레이션

ESP-IDF 3.5에서 5.5.0으로 업그레이드됨에 따라 다음 API 변경사항이 적용되었습니다:

#### 네트워크 인터페이스 변경

- **esp_netif**: 기존 tcpip_adapter를 대체하여 새로운 네트워크 인터페이스 API 사용
- **WiFi 드라이버**: `esp_wifi_*` 함수들이 esp_netif와 통합되도록 수정
- **이벤트 핸들러**: ESP-IDF 5.x 스타일의 이벤트 루프 및 핸들러로 변경

```cpp
// ESP-IDF 5.x 네트워크 초기화 패턴
esp_netif_init();
esp_event_loop_create_default();
esp_netif_create_default_wifi_sta();
```

#### DNS 설정 변경

- DNS 설정이 esp_netif 객체를 통해 직접 수행되도록 변경
- `esp_netif_dhcpc_option` 함수를 사용하여 DNS 서버 구성

#### 이벤트 시스템 변경

- 이벤트 핸들러 등록에 `esp_event_handler_register()` 대신 `esp_event_handler_instance_register()` 사용
- 이벤트 루프 생성 시 `esp_event_loop_create_default()` 표준 패턴 사용

#### 호환성 유지

- 기존 3.x API에서 5.x로의 마이그레이션 완료
- 모든 네트워크 기능이 5.x API로 정상 동작하도록 검증 완료

### PlatformIO

빌드 시스템 및 개발 환경 관리를 위한 도구입니다.

```ini
[env]
platform = espressif32@6.12.0
framework = espidf
board = EoRa_S3
board_build.flash_size = 4MB
upload_speed = 921600
```

### 컴파일러

- **C++**: C++17 표준
- **C**: C11 표준
- **工具链**: Xtensa ESP32-S3 GNU toolchain

---

## 주요 라이브러리

### RadioLib 7.4.0

LoRa 모듈 제어를 위한 라이브러리입니다.

```cpp
#include <RadioLib.h>
SX1262 radio = new Module(...);
```

지원 기능:
- FSK/LoRa 모뎀
- 주파수 대역: 923MHz (한국)
- 대역폭: 125kHz ~ 500kHz
- 확장 인자(SF): 7 ~ 12
- CRC 및 헤더 지원

### u8g2

OLED 디스플레이 드라이버 라이브러리입니다.

특징:
- 다양한 디스플레이 컨트롤러 지원
- 폰트 및 그래픽 기능
- ESP-IDF 포트 (u8g2-hal-esp-idf)

---

## 네트워킹

### 지원 프로토콜

| 프로토콜 | 포트 | 용도 |
|----------|------|------|
| UDP | 9910 | ATEM 스위처 통신 |
| TCP | 8099 | vMix 스위처 통신 |
| WebSocket | 4455 | OBS Studio 통신 (개발 중) |
| HTTP | 80 | 웹 설정 서버 |
| mDNS | 5353 | 로컬 서비스 디스커버리 |

### 네트워크 인터페이스

```
WiFi (Station 모드) ─┐
                      ├─── ESP32-S3 ──── LoRa (SX1262)
Ethernet (Optional) ─┘
```

---

## 소프트웨어 아키텍처

### 설계 패턴

1. **계층형 아키텍처**: 5계층 컴포넌트 구조
2. **인터페이스 기반 설계**: `ISwitcherPort` 등 추상 인터페이스
3. **이벤트 버스**: Pub/Sub 패턴 기반 컴포넌트 간 통신
4. **서비스 지향**: 각 서비스는 독립적인 FreeRTOS 태스크

### 메모리 관리

```cpp
// Packed 데이터 압축으로 메모리 절약
// 20채널 Tally = 5바이트 (채널당 2비트)
packed_data_t data;
packed_data_init(&data, 20);
```

### 동시성

모든 서비스는 독립적인 FreeRTOS 태스크에서 실행됩니다.

```cpp
xTaskCreate(network_service_task, "network", 4096, NULL, 5, NULL);
xTaskCreate(lora_service_task, "lora", 4096, NULL, 8, NULL);  // RX 모드: 우선순위 8
xTaskCreate(led_service_task, "led", 2048, NULL, 3, NULL);
```

### 우선순위 최적화 (RX 모드)

- **LoRa 태스크**: 우선순위 8로 상향하여 실시간 Tally 수신 보장
- **Event Bus**: RX 모드에서 우선순위 8로 동작하여 빠른 상태 전파
- **네트워크 태스크**: 스택 버퍼 최적화로 메모리 효율 개선

---

## 웹 UI 기술 스택

### 프론트엔드

| 기술 | 버전 | 용도 |
|------|------|------|
| TailwindCSS | 3.4.19 | 유틸리티 CSS 프레임워크 |
| DaisyUI | 5.5.14 | Tailwind 컴포넌트 라이브러리 |
| Alpine.js | Latest | 가벼운 리액티브 JS 프레임워크 |
| Lucide Icons | 0.562.0 | 아이콘 라이브러리 |

### 빌드 도구

```json
{
  "devDependencies": {
    "esbuild": "^0.20.0",
    "postcss": "^8.5.6",
    "postcss-cli": "^11.0.1",
    "tailwindcss": "^3.4.19",
    "autoprefixer": "^10.4.23"
  }
}
```

### 임베딩

빌드된 웹 리소스는 C 배열로 변환되어 펌웨어에 포함됩니다.

```cpp
// 자동 생성된 파일
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
```

---

## 스위처 프로토콜

### ATEM Protocol

Blackmagic Design ATEM 스위처용 UDP 프로토콜입니다.

- 패킷 구조: 헤더 + 길이 + 명령어 + 페이로드
- Tally 데이터: `TlSr` 커맨드에 포함
- 연결 유지: 주기적 하트비트 (ping-pong)

### vMix Protocol

vMix용 TCP 프로토콜입니다.

- 텍스트 기반 명령 프로토콜
- Tally XML 구독: `SUBSCRIBE TALLY`
- 연결 유지: 주기적 `PING` 명령

### LoRa Protocol

커스텀 LoRa 데이터 링크 프로토콜입니다.

```
[HEADER][TYPE][LENGTH][PAYLOAD][CRC]
```

- 암호화: AES-128 (옵션, 라이선스 기반)
- 압축: Packed Data (채널당 2비트)
- 확인 응답 (ACK) 메커니즘

---

## 개발 도구

### 필수 도구

| 도구 | 용도 |
|------|------|
| Python 3.8+ | PlatformIO 기반 환경 |
| PlatformIO CLI | 빌드 및 업로드 |
| VS Code + PlatformIO extension | 개발 환경 |
| esptool | 펌웨어 플래시 도구 |
| Node.js 18+ | 웹 UI 빌드 |

### 빌드 명령어

#### 1. 의존성 설치 (최초 1회)

```bash
# 가상환경 생성
python3 -m venv venv

# 가상환경 활성화
source venv/bin/activate

# PlatformIO 설치
pip install -U platformio
```

#### 2. 빌드

```bash
# 가상환경 활성화
source venv/bin/activate

# TX 환경 빌드
platformio run --environment eora_s3_tx

# RX 환경 빌드
platformio run --environment eora_s3_rx
```

#### 3. 업로드

```bash
# TX 환경 빌드 및 업로드
platformio run --environment eora_s3_tx -t upload

# RX 환경 빌드 및 업로드
platformio run --environment eora_s3_rx -t upload
```

#### 4. 디바이스 모니터링

**참고**: `pio device monitor`는 사용할 수 없습니다.

파이썬 스크립트로 포트를 종료하고 재부팅 후 모니터링을 진행해야 합니다.

---

## 의존성 관계

### 외부 라이브러리

```ini
[env]
lib_deps =
    jgromes/RadioLib@^7.4.0
```

### ESP-IDF 컴포넌트

프로젝트는 다음 ESP-IDF 컴포넌트를 사용합니다:

- `esp_wifi`
- `esp_eth`
- `esp_http_server`
- `nvs_flash`
- `freertos`

---

## 성능 최적화

### 전력 관리

- **Deep Sleep**: RX 모드에서 미사용 시 절전
- **WiFi 절전**: 주기적 절전 모드
- **LoRa 듀티 사이클**: 송수신 간격 최적화

### 메모리 최적화

- **Packed Data**: Tally 정보 압축 (채널당 2비트)
- **정적 리소스**: 웹 UI는 PSRAM 없이도 동작
- **태스크 스택 최적화**: 각 서비스별 스택 크기 조정

---

## 테스트 및 디버깅

### 로그 시스템

```cpp
#include "t_log.h"
T_LOGI(TAG, "Connection established");
T_LOGE(TAG, "Connection failed: %d", error);
T_LOGD(TAG, "Received %d bytes", len);
```

### 시리얼 모니터

```
monitor_speed = 921600
monitor_rts = 0
monitor_dtr = 0
```

---

## 버전 관리

### 펌웨어 버전

- 메이저: 하위 호환 불가 변경
- 마이너: 기능 추가
- 패치: 버그 수정

### 웹 UI 버전

`web/package.json`에서 관리하며, 펌웨어와 독립적으로 업데이트 가능합니다.

---

## 향후 로드맵

### 계획된 기능

- [ ] OBS Studio 프로토콜 완성
- [ ] TLS/HTTPS 지원
- [ ] OTA (Over-The-Air) 업데이트
- [ ] MQTT 브리지
- [ ] 다국어 UI 지원

### 개선 사항

- [ ] 전력 소모 최적화
- [ ] LoRa 통신 거리 개선
- [ ] 웹 UI 성능 향상
