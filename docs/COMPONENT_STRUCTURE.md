# 컴포넌트 구조

**작성일**: 2025-11-30
**버전**: 2.0 (기능별 단일 컴포넌트 구조)

---

## 개요

ESP-IDF 표준 컴포넌트 구조를 따르면서, 기능별로 그룹화하고 내부적으로 core/manager 계층을 분리한 구조입니다.

### 장점
- ✅ **기능별 응집도 높음**: 관련 코드가 한 곳에 모여 있음
- ✅ **ESP-IDF 호환**: 컴포넌트명 = 디렉토리명, flat 구조
- ✅ **내부 구조 명확**: core/manager 폴더로 계층 표현
- ✅ **include 경로 간단**: CMakeLists.txt에서 INCLUDE_DIRS로 관리
- ✅ **확장 용이**: 새 기능 추가 시 컴포넌트 하나만 추가

---

## 컴포넌트 목록 (7개)

### 1. system - 시스템 기본 기능
```
components/system/
├── CMakeLists.txt
├── common/           # 공통 유틸리티
│   ├── PinConfig.h         # 하드웨어 핀 맵
│   ├── CommonUtils.h       # 유틸리티 함수
│   ├── switcher_types.h    # Switcher 타입 정의
│   └── utils.h             # (deprecated) 하위 호환용
├── config/           # 설정 관리
│   ├── ConfigCore.h
│   └── ConfigCore.cpp
└── monitor/          # 시스템 모니터링
    ├── SystemMonitor.h
    └── SystemMonitor.cpp
```

**역할**: NVS 설정, 시스템 상태 모니터링, 공통 유틸리티
**의존성**: nvs_flash, switcher, json, esp_adc, esp_timer, esp_driver_tsens, driver

---

### 2. lora - LoRa 통신
```
components/lora/
├── CMakeLists.txt
├── core/             # 하드웨어 추상화
│   ├── LoRaCore.h
│   └── LoRaCore.cpp
├── manager/          # 통신 관리
│   ├── LoRaManager.h
│   └── LoRaManager.cpp
└── communication/    # 상위 비즈니스 로직
    ├── CommunicationManager.h
    └── CommunicationManager.cpp
```

**역할**: SX1262 LoRa 모듈 제어, 패킷 송수신, 통신 관리
**의존성**: system, switcher, esp_timer, spi_flash, driver

---

### 3. network - 네트워크
```
components/network/
├── CMakeLists.txt
├── core/             # WiFi/Ethernet Core
│   ├── WiFiCore.h
│   ├── WiFiCore.cpp
│   ├── EthernetCore.h
│   └── EthernetCore.cpp
└── manager/          # 네트워크 관리
    ├── NetworkManager.h
    └── NetworkManager.cpp
```

**역할**: WiFi AP/STA, W5500 Ethernet, 네트워크 상태 관리
**의존성**: system, esp_wifi, esp_eth, esp_netif

---

### 4. display - 디스플레이
```
components/display/
├── CMakeLists.txt
├── core/             # OLED Core
│   ├── OLEDCore.h
│   └── OLEDCore.cpp
└── manager/          # 화면 관리
    ├── DisplayManager.h
    └── DisplayManager.cpp
```

**역할**: SSD1306 OLED 제어, 화면 표시, UI 관리
**의존성**: system, driver, lora, network, switcher

---

### 5. interface_cli - CLI 인터페이스
```
components/interface_cli/
├── CMakeLists.txt
├── include/
│   └── CLICore.h
└── src/
    └── CLICore.cpp
```

**역할**: UART 시리얼 콘솔, 명령어 처리
**의존성**: system, driver, network, display, switcher

---

### 6. interface_web - 웹 인터페이스
```
components/interface_web/
├── CMakeLists.txt
├── convert_web_resources.py  # 웹 리소스 변환 스크립트
├── include/
│   ├── ApiHandler.h
│   └── WebServerCore.h
├── src/
│   ├── ApiHandler.cpp
│   ├── MonitorApi.cpp        # 시스템 모니터링 API
│   ├── MonitorApi.h
│   ├── WebServerCore.cpp
│   └── web_resources.c       # 임베디드 HTML/CSS/JS
└── www/                      # 웹 리소스 원본
    ├── index.html
    ├── style.css
    └── app.js
```

**역할**: HTTP 웹서버, REST API, 웹 UI
**의존성**: esp_http_server, network, system, json

---

### 7. switcher - Switcher 라이브러리
```
components/switcher/
├── CMakeLists.txt
├── common/
│   ├── include/
│   │   └── sw_platform.h
│   └── platform/
│       └── sw_platform_esp.c
├── handler/
│   ├── include/
│   │   └── switcher.h
│   └── src/
│       └── switcher.c
├── protocol/
│   ├── atem/
│   │   ├── include/
│   │   └── src/
│   ├── obs/
│   │   ├── include/
│   │   └── src/
│   └── vmix/
│       ├── include/
│       └── src/
└── manager/
    ├── include/
    │   └── SwitcherManager.h
    └── src/
        └── SwitcherManager.cpp
```

**역할**: ATEM/OBS/vMix 프로토콜, Dual Switcher 관리
**의존성**: system, lwip, esp_timer

---

## 의존성 그래프

```
interface_cli ─┐
interface_web ─┼─> display ──> lora ──> system
               │      │         │
               └──> network ────┘
                      │
                   switcher ──> system
```

**의존성 규칙**:
- `system`: 최하위 레이어, 다른 컴포넌트에 의존하지 않음
- `lora`, `network`: system에만 의존
- `display`: system, lora, network에 의존
- `interface_*`: 최상위 레이어, 모든 컴포넌트 사용 가능
- `switcher`: 독립적인 라이브러리, system에만 의존

---

## CMakeLists.txt 패턴

### 기본 패턴
```cmake
idf_component_register(
    SRCS
        "subdir1/file1.cpp"
        "subdir2/file2.cpp"
    INCLUDE_DIRS
        "subdir1"
        "subdir2"
    REQUIRES
        other_component1
        other_component2
)
```

### 주의사항
1. **SRCS**: 서브디렉토리 경로 포함
2. **INCLUDE_DIRS**: 서브디렉토리 경로 추가 (외부에서 include할 때 사용)
3. **REQUIRES**: 컴포넌트명만 (경로 구분자 `/` 사용 불가)
4. **컴포넌트명** = **디렉토리명** (ESP-IDF 제약사항)

---

## 변경 이력

### v2.0 (2025-11-30)
- 기능별 단일 컴포넌트 구조로 전환
- 내부 core/manager 계층 분리
- 컴포넌트 수 10개 → 7개로 감소
- 명명 규칙 통일 (interface_*)

### v1.0 (이전)
- Flat 구조, core_/manager_ prefix 사용
- 컴포넌트 수 많음, 관련 파일 분산

---

## 참고 문서
- ESP-IDF Component System: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html
- COMPONENT_RESTRUCTURE_PLAN.md: 리팩토링 계획 및 과정
