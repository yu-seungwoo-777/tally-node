# Tally Node - 프로젝트 구조

## 디렉토리 개요

```
tally-node/
├── src/                          # 메인 소스 코드
│   └── main.cpp                  # 진입점 (TX/RX 모드 분기)
├── components/                   # 5계층 컴포넌트 아키텍처
│   ├── 00_common/                # 공통 유틸리티 (모든 레이어에서 접근)
│   ├── 01_app/                   # 애플리케이션 계층
│   ├── 02_presentation/          # 프레젠테이션 계층
│   ├── 03_service/               # 서비스 계층 (비즈니스 로직)
│   ├── 04_driver/                # 드라이버 계층
│   └── 05_hal/                   # HAL 계층
├── web/                          # 웹 UI (DaisyUI + TailwindCSS)
├── boards/                       # PlatformIO 보드 정의
│   └── EoRa_S3.json             # EoRa-S3 보드 설정
├── examples/                     # 예제 프로젝트
├── .moai/                        # MoAI 설정 및 문서
└── platformio.ini                # PlatformIO 빌드 설정
```

## 5계층 컴포넌트 아키텍처

Tally Node는 계층별 명확한 책임 분리를 위해 5계층 구조를 따릅니다.

```
┌─────────────────────────────────────────────────────────┐
│                    01_app (Application)                  │
│         prod_tx_app / prod_rx_app                       │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│              02_presentation (UI/Display)                │
│    DisplayManager / WebServer / TxPage / RxPage         │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│              03_service (Business Logic)                 │
│  network / lora / led / switcher / device_manager       │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│               04_driver (Hardware Drivers)               │
│   lora / wifi / ethernet / display / switcher_driver    │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                 05_hal (Hardware Abstraction)            │
│         lora_hal / display_hal / wifi_hal               │
└─────────────────────────────────────────────────────────┘
```

### 의존성 방향

- **상향 의존**: 상위 계층이 하위 계층을 의존
- **00_common**: 모든 계층에서 접근 가능

---

## 00_common - 공통 컴포넌트

모든 레이어에서 공통으로 사용하는 유틸리티와 타입 정의입니다.

```
00_common/
├── tally_types/          # Tally 데이터 타입 정의
│   └── include/
│       ├── TallyTypes.h  # 공통 타입, 열거형, 구조체
│       └── PackedData.h  # Packed 데이터 처리
├── lora_protocol/        # LoRa 통신 프로토콜
│   └── include/
│       └── lora_protocol.h
├── event_bus/            # 이벤트 버스 (Pub/Sub)
│   └── include/
│       └── event_bus.h
├── t_log/                # 로깅 유틸리티
│   └── include/
│       └── t_log.h
└── app_types/            # 애플리케이션 공통 타입
    └── include/
        └── app_types.h
```

### 주요 타입 정의

| 타입 | 설명 |
|------|------|
| `tally_status_t` | Tally 상태 (OFF/PROGRAM/PREVIEW/BOTH) |
| `switcher_type_t` | 스위처 타입 (ATEM/vMix/OBS) |
| `connection_state_t` | 연결 상태 (DISCONNECTED/CONNECTED/...) |
| `packed_data_t` | 압축 Tally 데이터 구조체 |

### 컴포넌트 상세 기능

#### tally_types - Tally 공통 타입

**TallyTypes.h**: 스위처 컴포넌트에서 사용하는 모든 공통 타입과 인터페이스 정의

- **열거형 타입**:
  - `switcher_type_t`: ATEM, OBS, vMix 스위처 타입
  - `switcher_role_t`: Primary/Secondary 역할
  - `tally_network_if_t`: 네트워크 인터페이스 (Auto/Ethernet/WiFi)
  - `tally_status_t`: Tally 상태 (OFF/PROGRAM/PREVIEW/BOTH)
  - `connection_state_t`: 연결 상태 (DISCONNECTED/CONNECTING/CONNECTED/INITIALIZING/READY)

- **PackedData**: 채널당 2비트로 압축된 Tally 데이터
  - 20채널 = 5바이트로 최적화
  - C/C++ 이중 인터페이스 제공
  - `packed_data_to_uint64()`, `packed_data_from_uint64()` 직렬화 지원

- **ISwitcherPort 인터페이스**: 모든 스위처 드라이버가 구현해야 하는 공통 인터페이스
  - `initialize()`, `connect()`, `disconnect()`
  - `loop()`: 주기적 패킷 처리
  - `getPackedTally()`: 압축 Tally 데이터 조회
  - `setTallyCallback()`, `setConnectionCallback()`: 콜백 설정

#### event_bus - 이벤트 버스

**event_bus.h**: 레이어 간 결합도 제거를 위한 비동기 Pub/Sub 이벤트 시스템

- **단방향 통신**: 01_app → 02 → 03 → 04 → 05
- **내부 버퍼 방식**: 2048바이트 버퍼에 데이터 복사 (스택 변수 안전 사용)
- **이벤트 타입** (50+ 종류):
  - 시스템: `EVT_SYSTEM_READY`, `EVT_CONFIG_CHANGED`, `EVT_CONFIG_DATA_CHANGED`
  - LoRa: `EVT_LORA_RSSI_CHANGED`, `EVT_LORA_RX_STATUS_CHANGED`, `EVT_LORA_PACKET_RECEIVED`
  - 네트워크: `EVT_NETWORK_STATUS_CHANGED`, `EVT_NETWORK_RESTART_REQUEST`
  - 스위처: `EVT_SWITCHER_STATUS_CHANGED`, `EVT_TALLY_STATE_CHANGED`
  - 디바이스: `EVT_DEVICE_REGISTER`, `EVT_DEVICE_LIST_CHANGED`
  - 라이선스: `EVT_LICENSE_STATE_CHANGED`, `EVT_LICENSE_VALIDATE`

- **이벤트 데이터 구조체**:
  - `lora_rssi_event_t`: RSSI/SNR 정보
  - `switcher_status_event_t`: 듀얼모드, 연결 상태, Tally 데이터
  - `network_status_event_t`: WiFi AP/STA, Ethernet 상태
  - `config_data_event_t`: 전체 설정 데이터
  - `device_info_t`: 디바이스 정보 (ID, RSSI, 배터리, 카메라 ID)

#### app_types - 애플리케이션 공통 타입

**app_types.h**: 네트워크 설정, 시스템 설정 구조체

- `app_network_config_t`: WiFi AP/STA, Ethernet 설정
- `app_system_config_t`: 전역 시스템 설정

---

## 01_app - 애플리케이션 계층

시스템의 진입점이자 전체 애플리케이션 라이프사이클을 관리합니다.

```
01_app/
├── prod_tx_app/          # TX 모드 애플리케이션
│   └── include/
│       └── prod_tx_app.h
└── prod_rx_app/          # RX 모드 애플리케이션
    └── include/
        └── prod_rx_app.h
```

### 앱 라이프사이클

```cpp
// TX 모드
prod_tx_app_init()    // 초기화
prod_tx_app_start()   // 서비스 시작
    // ... 실행 ...
prod_tx_app_deinit()  // 정리

// RX 모드
prod_rx_app_init()
prod_rx_app_start()
    // ... 실행 ...
prod_rx_app_deinit()
```

### 컴포넌트 상세 기능

#### prod_tx_app - TX 모드 애플리케이션

**prod_tx_app.h**: 스위처 연결 및 LoRa 송신을 담당하는 TX 모드 앱

- **LoRa 설정 구조체** (`prod_tx_config_t`):
  - `frequency`: LoRa 주파수 (Hz)
  - `spreading_factor`: SF (7-12)
  - `coding_rate`: CR
  - `bandwidth`: 대역폭 (Hz)
  - `tx_power`: 송신 전력 (dBm)
  - `sync_word`: 동기 워드

- **생명주기 함수**:
  - `prod_tx_app_init()`: Event Bus, 각 서비스 초기화
  - `prod_tx_app_start()`: 모든 서비스 태스크 시작
  - `prod_tx_app_deinit()`: 리소스 정리

- **TX 모드 특화 기능**:
  - 스위처 Tally 데이터 수집 및 LoRa 브로드캐스트
  - 주기적 상태 요청 (device_manager 통해)
  - 웹 서버 운영

#### prod_rx_app - RX 모드 애플리케이션

**prod_rx_app.h**: LoRa 수신 및 Tally 표시를 담당하는 RX 모드 앱

- **RX 모드 특화 기능**:
  - LoRa 패킷 수신 및 Tally 데이터 파싱
  - LED/디스플레이 실시간 업데이트
  - 상태 요청 수신 시 응답 송신
  - 우선순위 8로 실시간 처리

- **전력 관리**:
  - Deep Sleep 모드 지원
  - 배터리 효율적 운영

---

## 02_presentation - 프레젠테이션 계층

UI/디스플레이 관련 컴포넌트를 포함합니다.

```
02_presentation/
├── display/                    # OLED 디스플레이 관리
│   ├── DisplayManager/         # 디스플레이 관리자
│   ├── pages/                  # 페이지 컴포넌트
│   │   ├── BootPage/           # 부트 페이지
│   │   ├── TxPage/             # TX 모드 페이지
│   │   └── RxPage/             # RX 모드 페이지
│   └── icons/                  # XBM 아이콘 데이터
└── web_server/                 # 임베디드 웹 서버
    ├── handlers/               # HTTP 요청 핸들러
    ├── static_embed/           # 정적 리소스 (임베디드)
    └── include/
        └── web_server.h
```

### 페이지 구조

| 페이지 | 설명 | 모드 |
|-------|------|------|
| BootPage | 부팅 화면, 초기화 진행률 표시 | 공통 |
| TxPage | 연결 상태, Tally 정보, IP 주소 표시 | TX |
| RxPage | 수신 신호 강도, 배터리, Tally 상태 표시 | RX |

### 컴포넌트 상세 기능

#### web_server - 임베디드 웹 서버

**web_server.h**: HTTP 서버 및 웹 UI 제공

- **라이프사이클**:
  - `web_server_init()`: 리소스 설정, URI 핸들러 등록
  - `web_server_start()`: HTTP 서버 실행
  - `web_server_stop()`: 서버 중지
  - `web_server_is_running()`: 상태 확인

- **주요 기능**:
  - 정적 리소스 임베딩 (index.html을 C 배열로 변환)
  - REST API 엔드포인트 제공
  - Event Bus 기반 실시간 상태 업데이트
  - 설정 저장/조회 API

- **서브모듈**:
  - `web_server_routes`: URI 라우팅
  - `web_server_json`: JSON 직렬화
  - `web_server_events`: SSE (Server-Sent Events)
  - `web_server_config`: 설정 관리
  - `web_server_cache`: 캐시 관리
  - `web_server_helpers`: 유틸리티 함수

#### display - OLED 디스플레이 관리

**DisplayManager**: u8g2 기반 디스플레이 제어

- **페이지 컴포넌트**:
  - `BootPage`: 초기화 진행률 표시 (0-100%)
  - `TxPage`:
    - 연결 상태 (WiFi/Ethernet/Switcher)
    - 현재 Tally 상태 (PGM/PVW 채널)
    - IP 주소 및 디바이스 ID
  - `RxPage`:
    - 수신 신호 강도 (RSSI/SNR)
    - 배터리 잔량 (%)
    - 마지막 수신 간격
    - 현재 Tally 상태

- **아이콘**: XBM 포맷 아이콘 데이터

---

## 03_service - 서비스 계층

비즈니스 로직을 담당하는 서비스 컴포넌트입니다.

```
03_service/
├── network_service/        # 네트워크 관리 (WiFi/Ethernet)
├── lora_service/           # LoRa 통신 서비스
├── led_service/            # LED 제어 서비스
├── switcher_service/       # 스위처 연결 서비스
├── device_manager/         # 장치 관리자
├── button_service/         # 버튼 입력 서비스
├── hardware_service/       # 하드웨어 모니터링
├── config_service/         # 설정 관리
├── license_service/        # 라이선스 관리
└── tally_test_service/     # Tally 테스트 서비스
```

### 서비스 역할

| 서비스 | 주요 책임 |
|--------|-----------|
| network_service | WiFi/Ethernet 연결, IP 할당, DNS |
| lora_service | LoRa 송수신, 패킷 파싱 |
| led_service | WS2812 LED 패턴 제어 |
| switcher_service | 스위처 프로토콜 처리 |
| device_manager | NVS 저장소 관리 |
| license_service | 라이선스 키 검증 및 관리 |
| tally_test_service | Tally 테스트 패턴 생성 |

### 컴포넌트 상세 기능

#### network_service - 네트워크 관리 서비스

**network_service.h**: WiFi와 Ethernet 통합 관리

- **네트워크 인터페이스 타입**:
  - `NETWORK_IF_WIFI_AP`: WiFi AP 모드
  - `NETWORK_IF_WIFI_STA`: WiFi Station 모드
  - `NETWORK_IF_ETHERNET`: Ethernet (W5500)

- **인터페이스 상태 구조체** (`network_if_status_t`):
  - `active`: 활성화 여부
  - `detected`: 하드웨어 감지 여부 (Ethernet만)
  - `connected`: 연결 상태
  - `ip`, `netmask`, `gateway`: 네트워크 정보

- **주요 기능**:
  - Event Bus 기반 설정 로드 (`EVT_CONFIG_DATA_CHANGED`)
  - 전체 상태 발행 (`EVT_NETWORK_STATUS_CHANGED`)
  - 네트워크 재시작 (WiFi/Ethernet/전체)
  - 상태 로그 출력

#### lora_service - LoRa 통신 서비스

**lora_service.h**: LoRa 송수신 및 RF 관리

- **LoRa 설정 구조체** (`lora_service_config_t`):
  - `frequency`: 주파수 (MHz)
  - `spreading_factor`: SF (7-12)
  - `coding_rate`: CR (5-8)
  - `bandwidth`: 대역폭 (kHz)
  - `tx_power`: 송신 전력 (dBm)
  - `sync_word`: 동기 워드

- **LoRa 상태 구조체** (`lora_service_status_t`):
  - `chip_type`: SX1262/SX1268
  - `rssi`, `snr`: 신호 강도
  - `packets_sent`, `packets_received`: 패킷 통계

- **주요 기능**:
  - 내부 FreeRTOS 태스크로 송신 큐 처리
  - Tally 데이터 송신 (`lora_service_send_tally()`)
  - 주파수 스캔 (`start_scan()`, `stop_scan()`)
  - Event Bus로 RSSI/SNR 발행

#### led_service - LED 제어 서비스

**led_service.h**: WS2812 LED + 내장 LED 제어

- **LED 색상 설정 구조체** (`led_colors_t`):
  - `program_r/g/b`: PROGRAM 색상
  - `preview_r/g/b`: PREVIEW 색상
  - `off_r/g/b`: OFF 색상

- **WS2812 LED 함수**:
  - `led_service_set_state()`: 상태 설정 (PROGRAM/PREVIEW/OFF/BATTERY_LOW)
  - `led_service_set_rgb()`: RGB 직접 설정
  - `led_service_set_brightness()`: 밝기 (0-255)
  - `led_service_set_camera_id()`: 카메라 ID 설정

- **내장 LED 함수**:
  - `led_service_board_led_on()`: 내장 LED 켜기
  - `led_service_board_led_off()`: 내장 LED 끄기
  - `led_service_toggle_board_led()`: 토글

#### switcher_service - 스위처 연결 서비스

**switcher_service.h**: 듀얼 스위처 관리 및 Tally 결합

- **스위처 역할**:
  - `SWITCHER_ROLE_PRIMARY`: Primary 스위처
  - `SWITCHER_ROLE_SECONDARY`: Secondary 스위처

- **C 인터페이스**:
  - `switcher_service_create()`: 서비스 생성
  - `switcher_service_set_atem()`: ATEM 스위처 설정
  - `switcher_service_set_dual_mode()`: 듀얼모드 설정
  - `switcher_service_set_secondary_offset()`: Secondary 오프셋
  - `switcher_service_get_combined_tally()`: 결합된 Tally 조회

- **C++ 인터페이스 (SwitcherService 클래스)**:
  - `setAtem()`, `setVmix()`: 스위처 설정
  - `start()`, `stop()`: 태스크 제어
  - `getCombinedTally()`: 듀얼모드 Tally 결합
  - `publishSwitcherStatus()`: Event Bus로 상태 발행

- **내부 동작**:
  - 내부 FreeRTOS 태스크 (8KB 스택)
  - 5초 간격 자동 재연결
  - 네트워크 IP 캐싱 (이벤트 기반 갱신)
  - Health refresh (1시간 무변화 시 강제 발행)
  - 듀얼모드 Packed 데이터 결합

#### device_manager - 장치 관리자

**device_manager.h**: TX/RX 디바이스 관리

- **TX 모드 기능**:
  - 주기적 상태 요청 (`set_request_interval()`)
  - 즉시 상태 요청 (`request_status_now()`)
  - PING 송신 (`send_ping()`)
  - 디바이스 리스트 관리 (최대 20대)

- **RX 모드 기능**:
  - 상태 요청 수신 시 응답 송신
  - 디바이스 등록/해제

#### config_service - 설정 관리

**config_service.h**: NVS 기반 설정 저장/로드

- **설정 항목**:
  - WiFi AP/STA 설정
  - Ethernet 설정
  - Switcher Primary/Secondary 설정
  - 듀얼모드, 오프셋 설정
  - 밝기, 카메라 ID, RF 설정

- **Event Bus 통합**:
  - `EVT_CONFIG_CHANGED`: 설정 저장 요청 수신
  - `EVT_CONFIG_DATA_REQUEST`: 설정 데이터 요청 수신
  - `EVT_CONFIG_DATA_CHANGED`: 설정 데이터 발행

#### license_service - 라이선스 관리

**license_service.h**: 라이선스 키 검증 및 관리

- **라이선스 상태**:
  - `LICENSE_STATE_INVALID`: 무효
  - `LICENSE_STATE_VALID`: 유효
  - `LICENSE_STATE_CHECKING`: 검증 중

- **주요 기능**:
  - 라이선스 키 검증
  - 디바이스 제한 확인
  - Event Bus로 상태 발행

#### button_service - 버튼 입력 서비스

GPIO 인터럽트 기반 버튼 처리

- **이벤트 발행**:
  - `EVT_BUTTON_SINGLE_CLICK`: 단일 클릭
  - `EVT_BUTTON_LONG_PRESS`: 롱프레스 시작 (1000ms)
  - `EVT_BUTTON_LONG_RELEASE`: 롱프레스 해제

#### hardware_service - 하드웨어 모니터링

**hardware_service.h**: 시스템 상태 모니터링

- **시스템 정보 이벤트** (`EVT_INFO_UPDATED`):
  - `device_id`: 디바이스 ID
  - `battery`: 배터리 %
  - `voltage`: 전압 (V)
  - `temperature`: 온도 (°C)
  - `uptime`: 업타임 (초)

#### tally_test_service - Tally 테스트 서비스

Tally 테스트 패턴 생성

- **테스트 모드 설정** (`EVT_TALLY_TEST_MODE_START`):
  - `max_channels`: 최대 채널 수 (1-20)
  - `interval_ms`: 송신 간격 (100-3000ms)

---

## 04_driver - 드라이버 계층

하드웨어 및 외부 시스템과의 인터페이스를 제공합니다.

```
04_driver/
├── lora_driver/            # LoRa 모듈 드라이버
├── wifi_driver/            # WiFi 드라이버
├── ethernet_driver/        # 이더넷 드라이버
├── display_driver/         # 디스플레이 드라이버
├── ws2812_driver/          # WS2812 LED 드라이버
├── battery_driver/         # 배터리 센서 드라이버
├── temperature_driver/     # 온도 센서 드라이버
├── board_led_driver/       # 보드 LED 드라이버
├── license_client/         # 라이선스 클라이언트
└── switcher_driver/        # 스위처 드라이버
    ├── atem/               # ATEM 프로토콜
    └── vmix/               # vMix 프로토콜
```

### 스위처 드라이버 인터페이스

모든 스위처 드라이버는 `ISwitcherPort` 인터페이스를 구현해야 합니다.

```cpp
class ISwitcherPort {
    virtual bool initialize() = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual int loop() = 0;
    virtual connection_state_t getConnectionState() const = 0;
    virtual packed_data_t getPackedTally() const = 0;
    // ... 기타 메서드
};
```

### 컴포넌트 상세 기능

#### lora_driver - LoRa 모듈 드라이버

**lora_driver.h**: RadioLib 기반 LoRa 제어

- **주요 기능**:
  - SX1262/SX1268 칩 초기화
  - 송수신 처리
  - RSSI/SNR 측정
  - 주파수 스캔

#### wifi_driver - WiFi 드라이버

**wifi_driver.h**: ESP-IDF WiFi 제어

- **ESP-IDF 5.5.0 호환**:
  - esp_netif API 사용
  - 이벤트 핸들러 인스턴스 기반

#### ethernet_driver - 이더넷 드라이버

**ethernet_driver.h**: W5500 칩 제어

- **ESP-IDF 5.5.0 호환**:
  - esp_netif API 사용
  - DNS 설정 방식 업데이트

#### display_driver - 디스플레이 드라이버

**display_driver.h**: u8g2 기반 OLED 제어

- **주요 기능**:
  - u8g2 초기화
  - 페이지 렌더링
  - 아이콘 표시

#### ws2812_driver - WS2812 LED 드라이버

**ws2812_driver.h**: RMT 기반 LED 제어

- **주요 기능**:
  - RGB 패턴 설정
  - 밝기 조절
  - 카메라 ID 표시

#### battery_driver - 배터리 센서 드라이버

ADC 기반 배터리 잔량 측정

#### temperature_driver - 온도 센서 드라이버

내부 온도 센서 읽기

#### board_led_driver - 보드 LED 드라이버

내장 LED 제어 (GPIO)

#### license_client - 라이선스 클라이언트

**license_client.h**: 라이선스 검증 클라이언트

- **주요 기능**:
  - 라이선스 키 검증 요청
  - 디바이스 제한 확인

#### switcher_driver - 스위처 드라이버

**atem/**: ATEM 프로토콜 구현
- UDP 포트 9910
- `TlSr` 커맨드 파싱
- 하트비트 유지

**vmix/**: vMix 프로토콜 구현
- TCP 포트 8099
- `SUBSCRIBE TALLY` 구독
- `PING` 명령 유지

---

## 05_hal - HAL 계층

가장 낮은 수준의 하드웨어 추상화 계층입니다.

```
05_hal/
├── lora_hal/               # LoRa HAL (RadioLib 래퍼)
│   └── include/
│       └── lora_hal.h
├── display_hal/            # 디스플레이 HAL
│   ├── u8g2_lib/           # u8g2 라이브러리
│   └── u8g2_hal/           # u8g2 ESP-IDF 포트
├── wifi_hal/               # WiFi HAL
├── ethernet_hal/           # 이더넷 HAL
├── battery_hal/            # 배터리 HAL
├── ws2812_hal/             # WS2812 HAL (RMT)
└── temperature_hal/        # 온도 센서 HAL
```

### 컴포넌트 상세 기능

#### lora_hal - LoRa HAL

**lora_hal.h**: RadioLib 래퍼

- **지원 칩**: SX1262 (868/915MHz), SX1268 (433MHz)
- **SPI 통신**: ESP32-S3 ↔ LoRa 모듈
- **주요 기능**:
  - 초기화 및 설정
  - 송수신
  - RSSI/SNR 측정

#### display_hal - 디스플레이 HAL

**u8g2_lib/**: u8g2 라이브러리 포트
- ESP-IDF용 I2C 드라이버
- 128x64 OLED 지원

**u8g2_hal/**: ESP-IDF 포팅
- I2C 마스터 초기화
- 버퍼 전송

#### wifi_hal - WiFi HAL

**wifi_hal.c**: ESP-IDF 5.5.0 WiFi HAL

- **ESP-IDF 5.5.0 마이그레이션**:
  - `esp_netif` API 사용 (tcpip_adapter 제거)
  - `esp_netif_set_dns_info()`로 DNS 설정
  - `esp_event_handler_instance_register()`로 이벤트 핸들러 등록

#### ethernet_hal - 이더넷 HAL

**ethernet_hal.c**: W5500 Ethernet HAL

- **ESP-IDF 5.5.0 마이그레이션**:
  - `esp_netif` API 사용
  - DNS 설정 방식 변경

#### battery_hal - 배터리 HAL

ADC 기반 배터리 전압 측정

- ADC 채널 설정
- 전압 → 백분율 변환

#### ws2812_hal - WS2812 HAL

RMT (Remote Control) 기반 LED 제어

- ESP32 RMT peripheral 사용
- NRZ (Non-Return-to-Zero) 인코딩

#### temperature_hal - 온도 센서 HAL

내부 온도 센서 읽기 (ESP32-S3 built-in)

---

## 웹 UI 구조

프론트엔드는 모듈식 esbuild 기반 빌드 시스템을 사용합니다.

```
web/
├── src/                    # 소스 코드
│   ├── css/                # 스타일시트
│   │   └── styles.css      # TailwindCSS + DaisyUI
│   └── js/                 # JavaScript (Alpine.js)
├── dist/                   # 빌드 결과 (임베디드용)
├── tools/                  # 빌드 스크립트
│   ├── build.js            # 번들러
│   ├── embed.js            # C 배열 변환
│   └── dev-server.js       # 개발 서버
├── package.json            # NPM 의존성
├── tailwind.config.js      # Tailwind 설정
└── postcss.config.js       # PostCSS 설정
```

### 웹 UI 기술 스택

- **CSS**: TailwindCSS + DaisyUI
- **JS**: Alpine.js (가벼운 리액티브 프레임워크)
- **빌드**: esbuild (고속 번들러)
- **아이콘**: Lucide Icons

---

## 빌드 시스템

### PlatformIO 환경

```
[env:eora_s3_tx]    # TX 모드
[env:eora_s3_rx]    # RX 모드
```

### 빌드 순서

1. **CMake 단계**: `CMakeLists.txt`가 모든 컴포넌트 등록
2. **컴포넌트 수집**: 각 레이어와 하위 컴포넌트를 자동 등록
3. **컴파일**: ESP-IDF 프로젝트 빌드
4. **링크**: 펌웨어 생성
5. **플래시**: 장치에 업로드

### 웹 UI 빌드

```bash
cd web/
npm install
npm run build    # dist/ 생성
npm run embed    # C 배열로 변환
npm run deploy   # ESP32 프로젝트로 복사
```
