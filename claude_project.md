# EoRa-S3 프로젝트 관리 문서

> **AI 어시스턴트는 작업 시작 전 이 문서를 반드시 확인하세요.**

## 프로젝트 정보
| 항목 | 내용 |
|------|------|
| 프로젝트명 | EoRa-S3 펌웨어 |
| 플랫폼 | ESP32-S3 |
| 프레임워크 | ESP-IDF 5.5.0 |
| PlatformIO | espressif32 6.12.0 |
| 보드 | EoRa-S3 (EBYTE) |

## 현재 상태
- **단계:** C→C++ 리팩토링 진행 중 (Phase 1 완료)
- **상태:** Phase 2 대기 (Core API 마이그레이션)
- **브랜치:** refactor/cpp-unified
- **언어:** C 87% + C++ 13% → C++ 75% + C 25% (목표)
- **구조:** components/ → src/{core,managers,network} (진행 중)
- **마지막 업데이트:** 2025-11-30

## 진행 중인 작업
- [x] ATEM 스위처 연결 및 Tally 수신
- [x] OLED PGM/PVW 표시
- [x] 멀티 인터페이스 시스템 계획 수립
- [x] Phase 1: 기반 인프라 (NVS + WiFi AP/STA)
- [x] Phase 2: W5500 이더넷
- [x] Phase 3: USB CDC CLI
- [x] Phase 4: HTTP 웹서버 및 REST API
- [x] LoRa 모듈 초기화 (SX1268, 433MHz)
- [x] LoRa 송신 테스트 완료
- [ ] Phase 6: 통합 테스트

## TODO - 멀티 인터페이스 시스템
### Phase 1: 기반 인프라 ✅ 완료
- [x] config_manager 모듈 (NVS 저장/로드)
- [x] wifi_manager 모듈 (AP+STA 동시)
- [x] network_manager 기본 구조

### Phase 2: W5500 이더넷 ✅ 완료
- [x] W5500 SPI 드라이버 초기화
- [x] DHCP/Static 자동 전환 로직
- [x] network_manager 통합

### Phase 3: USB CDC CLI ✅ 완료
- [x] USB Serial JTAG 초기화
- [x] CLI 파서 및 명령어 구현 (10개)

### Phase 4: 웹서버 ✅ 완료
- [x] HTTP 서버 + REST API
- [x] 웹 UI (HTML+JS)
- [x] 웹 리소스 자동 변환 시스템

### Phase 5: UDP 서버 ❌ 제거됨
- 웹서버 REST API로 충분하여 제거

### Phase 6: 통합 테스트
- [ ] 전체 통합 테스트

## TODO - 리팩토링 (2025-11-30 시작)
- [x] **Phase 1: 백업 및 브랜치 생성** ✅
  - [x] Git 커밋 및 태그 (v0.1-pre-refactor)
  - [x] 브랜치 생성 (refactor/cpp-unified)
  - [x] 폴더 구조 생성 (src/core, managers, network, protocol)
  - [x] utils.h 작성 (핀 맵, 유틸리티)
- [ ] **Phase 2: Core API 마이그레이션** (예상: 4시간)
  - [ ] ConfigCore (NVS)
  - [ ] WiFiCore (WiFi AP/STA)
  - [ ] EthernetCore (W5500)
  - [ ] LoRaCore (RadioLib)
  - [ ] OLEDCore (SSD1306)
  - [ ] CLICore (esp_console)
- [ ] **Phase 3: Manager 마이그레이션** (예상: 3시간)
  - [ ] NetworkManager (WiFi+Eth+WebServer)
  - [ ] CommunicationManager (LoRa+Switcher)
  - [ ] DisplayManager (OLED+Tally)
- [ ] **Phase 4: main.cpp 통합** (예상: 1시간)
- [ ] **Phase 5: 빌드 및 테스트** (예상: 3시간)
  - [ ] CMakeLists.txt 수정
  - [ ] 빌드 성공
  - [ ] 기능 테스트 (WiFi, LoRa, ATEM, OLED, CLI)

## TODO - 기타
- [ ] 다른 스위처 테스트 (OBS, vMix)
- [ ] 에러 처리 및 재연결 로직

---

## 문서 구조 (Documents)

| 파일명 | 위치 | 역할 |
|--------|------|------|
| CLAUDE.md | /home/dev/esp-idf/ | AI 어시스턴트 지침, 개발 규칙 |
| claude_project.md | /home/dev/esp-idf/ | 프로젝트 작업 관리, 히스토리 |
| platformio.ini | /home/dev/esp-idf/ | PlatformIO 빌드 설정 |
| EoRa_S3.json | /home/dev/esp-idf/boards/ | 보드 하드웨어 설정 |
| MULTI_INTERFACE_SYSTEM_PLAN.md | /home/dev/esp-idf/docs/ | 멀티 인터페이스 시스템 구현 계획 |
| ATEM_LIBRARY_COMPARISON.md | /home/dev/esp-idf/docs/ | ATEM 라이브러리 비교 분석 |
| WEBSERVER_DEVELOPMENT.md | /home/dev/esp-idf/docs/ | 웹서버 개발 가이드 (자동화, API 문서) |
| LORA_SPI_TROUBLESHOOTING.md | /home/dev/esp-idf/docs/ | LoRa SPI 통신 문제 해결 가이드 |
| CLI_USAGE_GUIDE.md | /home/dev/esp-idf/docs/ | CLI 명령어 사용 가이드 (전체 명령어, 예시) |
| SPI_INITIALIZATION_GUIDE.md | /home/dev/esp-idf/docs/ | SPI 초기화 가이드 (LoRa, W5500 설정) |
| CPP_REFACTORING_ANALYSIS.md | /home/dev/esp-idf/docs/ | C→C++ 장단점 분석 (Switcher 라이브러리 호환성) |
| REFACTORING_PLAN.md | /home/dev/esp-idf/docs/ | C→C++ 리팩토링 계획서 v1 (아키텍처, 폴더 구조) |
| REFACTORING_PLAN_V2.md | /home/dev/esp-idf/docs/ | **C++ 리팩토링 최종 계획서 (Core API + Manager 구조)** |
| MIGRATION_GUIDE.md | /home/dev/esp-idf/docs/ | 마이그레이션 실행 가이드 (단계별 작업 매뉴얼) |
| switcher_architecture.md | /home/dev/esp-idf/docs/ | **스위처 시스템 아키텍처 (계층 구조, Tally Packed, 데이터 흐름)** |
| refactoring_plan.md | /home/dev/esp-idf/docs/ | **역할 기반 아키텍처 리팩토링 계획서 (2025-12-12)** |

---

## 소스 코드 구조

### 메인 소스
| 파일명 | 위치 | 역할 |
|--------|------|------|
| main.c | /home/dev/esp-idf/src/ | 메인 엔트리 포인트 |

### 컴포넌트 (components/)
| 컴포넌트 | 위치 | 역할 | 상태 |
|---------|------|------|------|
| config_manager | /home/dev/esp-idf/components/config_manager/ | NVS 기반 설정 관리 | ✅ Phase 1 |
| wifi_manager | /home/dev/esp-idf/components/wifi_manager/ | WiFi AP+STA 관리 | ✅ Phase 1 |
| eth_manager | /home/dev/esp-idf/components/eth_manager/ | W5500 이더넷 관리 | ✅ Phase 2 |
| network_manager | /home/dev/esp-idf/components/network_manager/ | 네트워크 통합 관리자 | ✅ Phase 1+2 |
| cli_handler | /home/dev/esp-idf/components/cli_handler/ | USB CDC CLI | ✅ Phase 3 |
| webserver | /home/dev/esp-idf/components/webserver/ | HTTP 웹서버 및 REST API | ✅ Phase 4 |
| lora_manager | /home/dev/esp-idf/components/lora_manager/ | LoRa 무선 통신 (SX1268) | ✅ 초기화 완료 |
| ssd1306 | /home/dev/esp-idf/components/ssd1306/ | OLED 디스플레이 드라이버 | ✅ 기존 |

---

## 개발 히스토리 (History)

### 2025-12-12: 역할 기반 아키텍처 리팩토링 계획 완료
- **작업:** 전체 시스템 리팩토링 계획 수립
- **내용:**
  - 역할 기반 컴포넌트 분류 (Application, Domain Manager, Core Service, Infrastructure)
  - InfoManager 컴포넌트 설계 (장치 ID, 시스템 정보 중앙 관리)
  - C/C++ 하이브리드 전략 수립 (신규 기능 C++, 하드웨어 C 유지)
  - 계층별 설계 원칙 정의
  - 단계적 이전 계획 수립 (1단계: InfoManager → 4단계: 점진적 확장)
- **문서:** `refactoring_plan.md`
- **기술:**
  - PIMPL 패턴 활용
  - Observer 패턴 적용
  - RAII와 스마트 포인터 활용
  - ESP-IDF 컴포넌트 규칙 준수

### 2025-12-11: LoRa RSSI/SNR 디스플레이 통합 완료
- **작업:** LoRa 신호 강도 정보 실시간 디스플레이 연동
- **내용:**
  - DisplayManager에 LoRa RSSI/SNR 필드 추가
  - LoRaCore에 패킷 수신 시 신호 정보 저장 기능 구현
  - TxPage/RxPage에서 실제 LoRa 데이터 사용하도록 수정
  - 5초 주기 디스플레이 업데이트 체계 안정화
  - SettingsPage는 이벤트 기반 동작으로 주기적 업데이트 불필요 확인
- **파일:**
  - `/components/display/src/DisplayManager.cpp`
  - `/components/lora/core/LoRaCore.cpp`
  - `/components/display/src/TxPage.c`
  - `/components/display/src/RxPage.c`
- **기술:**
  - LoRaManager-C wrapper 연동
  - DisplayManager 중앙 관리 아키텍처
  - 메모리 안정성 확보 (중복 업데이트 제거)
  - I2C 통신 최적화
- **커밋:** `86ecb42`

### 2025-12-01
| 시간 | 작업 내용 | 커밋 |
|------|----------|------|
| - | **스위처 Tally 채널 수 20채널 통일** | `0c228a7` |
| - | SWITCHER_MAX_CHANNELS (20채널) 상수 정의 | `0c228a7` |
| - | ATEM/OBS/vMix 모두 동일한 상수명 사용 | `0c228a7` |
| - | 하드 제한: 20채널 (절대 초과 불가) | `0c228a7` |
| - | 사용자 제한 정책 적용 (user_limit 반영) | `0c228a7` |
| - | effective_camera_limit 계산 로직 단순화 | `0c228a7` |
| - | **스위처 시스템 아키텍처 문서 작성** | - |
| - | switcher_architecture.md 문서 생성 | - |
| - | 계층별 상세 설명 (Manager, Handler, Protocol) | - |
| - | Tally Packed 시스템 완벽 정리 | - |
| - | Combine 로직 분석 (비트 OR 방식) | - |
| - | 데이터 흐름 및 설계 원칙 문서화 | - |

### 2025-11-29
| 시간 | 작업 내용 | 커밋 |
|------|----------|------|
| - | 스위처 통합 라이브러리 연동 (ATEM/OBS/vMix) | `59e83c3` |
| - | 스위처 라이브러리 심볼릭 링크로 변경 | `c6b88fb` |
| - | 통신 속도 최적화 (lwIP, 코어 분리, I2C 1MHz) | `23b0e5e` |
| - | sw_debug 제거, ESP 네이티브 로깅 사용 | - |
| - | Tally unpack/format 함수 추가, 다중 채널 표시 | `13717c1` |
| - | **멀티 인터페이스 시스템 계획 수립** | - |
| - | MULTI_INTERFACE_SYSTEM_PLAN.md 문서 작성 | - |
| - | **Phase 1 완료: 기반 인프라 구현** | `1da629a` |
| - | config_manager 컴포넌트 개발 (NVS 저장/로드) | `1da629a` |
| - | wifi_manager 컴포넌트 개발 (WiFi AP+STA 동시) | `1da629a` |
| - | network_manager 컴포넌트 개발 (통합 관리) | `1da629a` |
| - | main.c에 network_manager 통합 | `1da629a` |
| - | 빌드 성공 (814KB Flash, 35KB RAM) | `1da629a` |
| - | Utils.h 추가 (EoRa-S3 핀 맵 정의) | `23fb2cd` |
| - | **WiFi + ATEM 통신 테스트 성공** | `555edc0` |
| - | WiFi STA 연결 대기 로직 추가 | `555edc0` |
| - | 로그 레벨 INFO로 변경 (디버깅) | `555edc0` |
| - | ATEM Mini 연결 및 Tally 수신 확인 | `555edc0` |
| - | **Phase 2 완료: W5500 이더넷 구현** | - |
| - | eth_manager 컴포넌트 개발 (W5500 SPI 드라이버) | - |
| - | DHCP/Static IP 자동 전환 로직 (10초 타임아웃) | - |
| - | network_manager에 eth_manager 통합 | - |
| - | SPI3_HOST 사용 (LoRa와 버스 분리) | - |
| - | sdkconfig W5500 지원 활성화 | - |
| - | 빌드 성공 (897KB Flash, 36KB RAM) | - |
| - | **Phase 4 완료: HTTP 웹서버 및 REST API** | `6846250` |
| - | webserver 컴포넌트 개발 (esp_http_server) | `6846250` |
| - | REST API 엔드포인트 구현 (status, config, restart) | `6846250` |
| - | 웹 UI 개발 (HTML/CSS/JavaScript, 반응형) | `6846250` |
| - | 웹 리소스 자동 변환 시스템 (pre_build.py) | `6846250` |
| - | convert_web_resources.py (HTML→C 문자열) | `6846250` |
| - | PlatformIO pre-build hook 통합 | `6846250` |
| - | WEBSERVER_DEVELOPMENT.md 문서 추가 | `6846250` |
| - | 빌드 성공 (966KB Flash, 36KB RAM) | `6846250` |
| - | **Phase 3 완료: USB CDC CLI** | `52060c3` |
| - | cli_handler 컴포넌트 개발 (esp_console) | `52060c3` |
| - | 시스템 명령어 구현 (help, status, reboot) | `52060c3` |
| - | WiFi 명령어 구현 (status, scan, set) | `52060c3` |
| - | Ethernet 명령어 구현 (status, dhcp, static) | `52060c3` |
| - | 설정 명령어 구현 (config.show) | `52060c3` |
| - | WiFi Manager 스캔 함수 추가 | `52060c3` |
| - | ESP32-S3 USB Serial JTAG 사용 | `52060c3` |
| - | 빌드 성공 (999KB Flash, 36KB RAM) | `52060c3` |
| - | **LoRa 모듈 초기화 완료 (SX1268, 433MHz)** | - |
| - | lora_manager 컴포넌트 개발 (RadioLib 7.4.0) | - |
| - | ESP-IDF SPI HAL 구현 (SPI2_HOST) | - |
| - | SX1268 칩 감지 및 초기화 성공 | - |
| - | TCXO/XTAL 설정 해결 (tcxoVoltage=0) | - |
| - | BUSY timeout 경고 해결 (10ms) | - |
| - | LoRa 파라미터: SF7, BW125kHz, CR4/7, 22dBm | - |
| - | 빌드 성공 (1020KB Flash, 36KB RAM) | - |
| - | **LoRa 송신 테스트 완료** | - |
| - | CLI에 lora.tx 명령어 추가 (이미 구현됨) | - |
| - | 송신 테스트 성공 (11 bytes "Hello LoRa!") | - |
| - | 최종 빌드: 1026KB Flash, 36KB RAM | - |
| - | **CLI 사용 가이드 작성** | - |
| - | CLI_USAGE_GUIDE.md 문서 작성 | - |
| - | 전체 12개 명령어 사용법 상세 설명 | - |
| - | 접속 방법, 사용 예시, 문제 해결 포함 | - |
| - | **SPI 초기화 가이드 작성** | - |
| - | SPI_INITIALIZATION_GUIDE.md 문서 작성 | - |
| - | LoRa/W5500 SPI 초기화 방법 종합 정리 | - |
| - | 하드웨어 구성, 핀 맵, 문제 해결 포함 | - |

### 2025-11-25
| 시간 | 작업 내용 | 커밋 |
|------|----------|------|
| 23:20 | PlatformIO + ESP-IDF 환경 구축 | - |
| 23:30 | 테스트 펌웨어 작성 및 빌드 성공 | - |
| 23:35 | 펌웨어 업로드 및 시리얼 모니터 테스트 완료 | - |
| 23:43 | espressif32 6.12.0 (ESP-IDF 5.5.0) 업그레이드 | - |
| 23:50 | CLAUDE.md, claude_project.md 문서 작성 | - |
| 23:55 | Git 저장소 초기화, .gitignore 설정 | `2b26460` |

---

## 하드웨어 정보

### EoRa-S3 보드 스펙
- **MCU:** ESP32-S3 (240MHz, Dual Core)
- **Flash:** 4MB
- **PSRAM:** 2MB
- **무선:** WiFi, BLE
- **LoRa:** SX1262 (별도 확인 필요)
- **디스플레이:** OLED (별도 확인 필요)

### 핀 맵 (추후 업데이트)
```
// TODO: 핀 맵 정의
```

---

## 참고 사항
- 이전 Arduino 프로젝트 참고 파일: `/home/dev/esp-idf/src_backup/` (필요시 생성)
- 빌드 시 Flash size 경고 발생 (4MB 설정이나 2MB로 감지) - 동작에는 문제 없음
