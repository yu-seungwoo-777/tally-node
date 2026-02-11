# SPEC-CONFIG-RESTART-001: 설정 변경 감지 및 재시작 분석

## TAG BLOCK

```
SPEC-ID: SPEC-CONFIG-RESTART-001
TITLE: Config Change Detection and Restart Analysis
DOMAIN: CONFIG, RESTART, NETWORK
STATUS: PLANNED
PRIORITY: HIGH
ASSIGNED: manager-spec
CREATED: 2026-02-11
VERSION: 1.0.0
```

## 환경 (Environment)

### 대상 시스템
- **프로젝트**: Tally Node (ESP32 기반 Tally 시스템)
- **주요 구성요소**:
  - NetworkService: WiFi/Ethernet 네트워크 관리
  - LoRaService: LoRa 무선 통신 관리
  - SwitcherService: ATEM/vMix 스위처 연결 관리
  - ConfigService: NVS 설정 저장 및 이벤트 발행

### 영향받는 컴포넌트
- `components/03_service/network_service/network_service.cpp`
- `components/03_service/lora_service/lora_service.cpp`
- `components/03_service/switcher_service/switcher_service.cpp`
- `components/03_service/config_service/config_service.cpp`

### 기술 스택
- **언어**: C++
- **플랫폼**: ESP-IDF (FreeRTOS)
- **이벤트 버스**: event_bus (publish/subscribe 패턴)
- **설정 저장**: NVS (Non-Volatile Storage)

---

## 가정 (Assumptions)

### 검증된 가정 (Confidence: High)
1. 사용자가 웹 인터페이스에서 설정을 변경하면 `EVT_CONFIG_DATA_CHANGED` 이벤트가 발행됨
2. 각 서비스는 이벤트 버스를 통해 설정 변경을 수신
3. 설정 변경 후 드라이버 재초기화가 필요한 경우가 있음

### 검증 필요 가정 (Confidence: Medium)
1. 모든 네트워크 관련 설정 변경이 재시작을 필요로 하지는 않을 것임
2. 일부 설정 변경은 즉시 적용 가능하거나 별도 처리가 필요할 수 있음
3. 현재 구현이 모든 필수 설정 변경을 감지하지 못할 수 있음

### 리스크 가정 (Risk if Wrong)
1. 잘못된 가정으로 인해 네트워크가 불안정해질 수 있음
2. 사용자가 기기 재부팅 없이 설정을 적용할 수 없게 될 수 있음
3. 일부 설정 변경이 누락되어 의도치 않은 동작이 발생할 수 있음

---

## 요구사항 (Requirements - EARS 형식)

### 1. Ubiquitous Requirements (항상 활성화되는 요구사항)

**REQ-CONFIG-001**: 시스템은 **항상** 설정 변경 이벤트를 수신할 준비가 되어야 한다.

**REQ-CONFIG-002**: 시스템은 **항상** 설정 변경 감지를 위해 정적 변수 상태를 올바르게 관리해야 한다.

### 2. Event-Driven Requirements (이벤트 기반 요구사항)

**REQ-CONFIG-101**: **WHEN** `EVT_CONFIG_DATA_CHANGED` 이벤트가 수신되면, 시스템은 **항상** 관련 설정 값의 변경 여부를 확인해야 한다.

**REQ-CONFIG-102**: **WHEN** Ethernet `dhcp_enabled` 설정이 변경되면, 시스템은 **항상** 네트워크를 재시작해야 한다.

**REQ-CONFIG-103**: **WHEN** Ethernet `static_ip`, `static_netmask`, `static_gateway` 설정이 변경되면, 시스템은 **항상** 네트워크를 재시작해야 한다.

**REQ-CONFIG-104**: **WHEN** WiFi `ssid` 또는 `password` 설정이 변경되면, 시스템은 **항상** WiFi 연결을 재시작해야 한다.

**REQ-CONFIG-105**: **WHEN** LoRa `frequency` 또는 `sync_word` 설정이 변경되면, 시스템은 **항상** 드라이버에 새 설정을 적용해야 한다.

**REQ-CONFIG-106**: **WHEN** Switcher IP, Port, 또는 Interface 설정이 변경되면, 시스템은 **항상** 스위처 연결을 재설정해야 한다.

### 3. State-Driven Requirements (상태 기반 요구사항)

**REQ-CONFIG-201**: **IF** 드라이버가 이미 초기화된 상태에서 설정이 변경되면, 시스템은 변경 감지 로직을 수행해야 한다.

**REQ-CONFIG-202**: **IF** 드라이버가 초기화되지 않은 상태에서 설정이 변경되면, 시스템은 초기화 로직을 수행해야 한다.

**REQ-CONFIG-203**: **IF** 설정 변경이 감지되지 않은 값뿐이면, 시스템은 불필요한 재시작을 수행하지 않아야 한다.

### 4. Unwanted Behavior Requirements (금지 동작 요구사항)

**REQ-CONFIG-301**: 시스템은 설정 변경 감지를 위한 정적 변수가 **초기화되지 않은 상태로 사용되어서는 안 된다**.

**REQ-CONFIG-302**: 시스템은 **설정 변경을 누락해서는 안 된다**.

**REQ-CONFIG-303**: 시스템은 **불필요한 네트워크 재시작을 수행해서는 안 된다**.

### 5. Optional Requirements (선택적 요구사항)

**REQ-CONFIG-401**: **가능하면** 설정 변경 시 재시작이 필요한지 여부를 사용자에게 미리 안내해야 한다.

**REQ-CONFIG-402**: **가능하면** 설정 변경 로그를 상세히 기록하여 디버깅을 지원해야 한다.

---

## 명세 (Specifications)

### SPEC-001: NetworkService 설정 변경 감지

**현재 구현 분석**:

```cpp
// network_service.cpp:714-721
static bool last_eth_enabled = false;
bool eth_changed = (last_eth_enabled != s_config.ethernet.enabled);
last_eth_enabled = s_config.ethernet.enabled;

if (eth_changed) {
    T_LOGI(TAG, "Ethernet config changed (enabled=%d), restarting...", s_config.ethernet.enabled);
    restartEthernet();
}
```

**문제점**:
1. `ethernet.enabled` 변경만 감지하고 있음
2. `ethernet.dhcp_enabled` 변경 감지 누락
3. `ethernet.static_ip`, `ethernet.static_netmask`, `ethernet.static_gateway` 변경 감지 누락

**요구되는 동작**:

```cpp
// 개선된 변경 감지
static bool last_eth_enabled = false;
static bool last_eth_dhcp_enabled = false;
static char last_eth_static_ip[16] = "";
static char last_eth_static_netmask[16] = "";
static char last_eth_static_gateway[16] = "";

bool eth_enabled_changed = (last_eth_enabled != s_config.ethernet.enabled);
bool eth_dhcp_changed = (last_eth_dhcp_enabled != s_config.ethernet.dhcp_enabled);
bool eth_ip_changed = (strcmp(last_eth_static_ip, s_config.ethernet.static_ip) != 0);
bool eth_netmask_changed = (strcmp(last_eth_static_netmask, s_config.ethernet.static_netmask) != 0);
bool eth_gateway_changed = (strcmp(last_eth_static_gateway, s_config.ethernet.static_gateway) != 0);

// 상태 업데이트
last_eth_enabled = s_config.ethernet.enabled;
last_eth_dhcp_enabled = s_config.ethernet.dhcp_enabled;
strncpy(last_eth_static_ip, s_config.ethernet.static_ip, sizeof(last_eth_static_ip) - 1);
strncpy(last_eth_static_netmask, s_config.ethernet.static_netmask, sizeof(last_eth_static_netmask) - 1);
strncpy(last_eth_static_gateway, s_config.ethernet.static_gateway, sizeof(last_eth_static_gateway) - 1);

// 재시작 조건
bool need_restart = eth_enabled_changed || eth_dhcp_changed || eth_ip_changed || eth_netmask_changed || eth_gateway_changed;

if (need_restart) {
    T_LOGI(TAG, "Ethernet config changed, restarting...");
    restartEthernet();
}
```

### SPEC-002: WiFi 설정 변경 감지

**현재 구현 분석**:
- WiFi 설정 변경 감지 로직이 `onConfigDataEvent`에 구현되어 있지 않음
- WiFi 재시작은 `onRestartRequest` 이벤트로만 처리됨

**요구되는 동작**:

```cpp
// WiFi 설정 변경 감지 추가
static bool last_wifi_ap_enabled = false;
static char last_wifi_sta_ssid[33] = "";
static char last_wifi_sta_password[65] = "";

bool wifi_ap_changed = (last_wifi_ap_enabled != s_config.wifi_ap.enabled);
bool wifi_sta_ssid_changed = (strcmp(last_wifi_sta_ssid, s_config.wifi_sta.ssid) != 0);
bool wifi_sta_password_changed = (strcmp(last_wifi_sta_password, s_config.wifi_sta.password) != 0);

// 상태 업데이트
last_wifi_ap_enabled = s_config.wifi_ap.enabled;
strncpy(last_wifi_sta_ssid, s_config.wifi_sta.ssid, sizeof(last_wifi_sta_ssid) - 1);
strncpy(last_wifi_sta_password, s_config.wifi_sta.password, sizeof(last_wifi_sta_password) - 1);

// 재시작 조건
bool need_wifi_restart = wifi_ap_changed || wifi_sta_ssid_changed || wifi_sta_password_changed;

if (need_wifi_restart) {
    T_LOGI(TAG, "WiFi config changed, restarting...");
    restartWiFi();
}
```

### SPEC-003: LoRaService RF 설정 변경 감지

**현재 구현 분석**:

```cpp
// lora_service.cpp:152-169 (TX 모드)
bool values_changed = (!s_rf_initialized ||
                       s_last_frequency != rf->frequency ||
                       s_last_sync_word != rf->sync_word);
```

**평가**: LoRaService는 올바르게 구현됨
- `frequency` 및 `sync_word` 변경 감지
- 초기화 상태 플래그(`s_rf_initialized`)로 부팅 시 불필요한 broadcast 방지

**요구사항**: 현재 구현 유지 (개선 불필요)

### SPEC-004: SwitcherService 설정 변경 감지

**현재 구현 분석**:

```cpp
// switcher_service.cpp:684-701
bool dual_changed = (config->dual_enabled != dual_mode_enabled_);
bool offset_changed = (config->secondary_offset != secondary_offset_);

// switcher_service.cpp:712-760
bool type_changed = (current_type != config->primary_type);
bool ip_changed = (strncmp(config->primary_ip, primary_.ip, sizeof(config->primary_ip)) != 0);
bool port_changed = (config->primary_port != primary_.port);
bool interface_changed = (config->primary_interface != primary_.network_interface);
bool camera_limit_changed = (config->primary_camera_limit != primary_.camera_limit);
```

**평가**: SwitcherService는 포괄적으로 구현됨
- 듀얼 모드, 오프셋 변경 감지
- Primary/Secondary 각각에 대한 type, IP, Port, Interface, Camera Limit 변경 감지
- camera_limit 변경은 재연결 없이 처리

**요구사항**: 현재 구현 유지 (개선 불필요)

---

## 추적 가능성 (Traceability)

### 관련 파일
- `components/03_service/network_service/network_service.cpp`
  - `NetworkServiceClass::onConfigDataEvent()` (라인 635-725)
  - `NetworkServiceClass::restartEthernet()` (라인 541-575)
  - `NetworkServiceClass::restartWiFi()` (라인 448-539)

- `components/03_service/lora_service/lora_service.cpp`
  - `on_rf_changed()` (라인 139-207)

- `components/03_service/switcher_service/switcher_service.cpp`
  - `onConfigDataEvent()` (라인 130-143)
  - `checkConfigAndReconnect()` (라인 616-823)

- `components/03_service/config_service/config_service.cpp`
  - `EVT_CONFIG_DATA_CHANGED` 이벤트 발행

### 관련 이벤트
- `EVT_CONFIG_DATA_CHANGED`: 설정 데이터 변경 이벤트
- `EVT_NETWORK_RESTART_REQUEST`: 네트워크 재시작 요청 이벤트
- `EVT_RF_CHANGED`: RF 설정 변경 이벤트

### 관련 구조체
- `app_network_config_t`: 네트워크 설정 구조체
- `config_ethernet_t`: Ethernet 설정
- `config_wifi_ap_t`: WiFi AP 설정
- `config_wifi_sta_t`: WiFi STA 설정
- `lora_rf_event_t`: LoRa RF 설정 이벤트
- `config_data_event_t`: 설정 데이터 이벤트

---

## 검증 고려사항 (Verification Considerations)

### 테스트 시나리오
1. **Ethernet DHCP/Static 변경**: 웹 인터페이스에서 DHCP 모드와 Static 모드를 전환
2. **Ethernet Static IP 변경**: Static IP 주소, 서브넷 마스크, 게이트웨이 변경
3. **WiFi SSID/Password 변경**: WiFi STA SSID 또는 비밀번호 변경
4. **LoRa 주파수 변경**: LoRa 주파수 또는 sync word 변경
5. **Switcher 설정 변경**: ATEM/vMix IP, Port, Interface 변경

### 검증 방법
- 유닛 테스트: 각 서비스의 설정 변경 감지 로직 테스트
- 통합 테스트: 웹 인터페이스에서 설정 변경 후 네트워크 상태 확인
- 수동 테스트: 실제 기기에서 설정 변경 및 재부팅 없이 동작 확인

---

## 참고 (References)

- `.moai/project/product.md`: 제품 요구사항
- `.moai/project/tech.md`: 기술 스택
- `components/00_common/app_types/include/app_types.h`: 공통 타입 정의
- `components/00_common/event_bus/include/event_bus.h`: 이벤트 버스 API
