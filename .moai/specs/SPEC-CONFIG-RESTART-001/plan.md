# SPEC-CONFIG-RESTART-001: 구현 계획

## TAG BLOCK

```
SPEC-ID: SPEC-CONFIG-RESTART-001
TITLE: Config Change Detection and Restart Analysis Implementation Plan
DOMAIN: CONFIG, RESTART, NETWORK
STATUS: PLANNED
PHASE: PLAN
ASSIGNED: TBD
CREATED: 2026-02-11
VERSION: 1.0.0
TRACEABILITY: SPEC-CONFIG-RESTART-001
```

---

## 구현 개요 (Implementation Overview)

이 SPEC은 코드베이스 전체에서 설정 변경 감지 및 재시작 로직을 분석하고, 누락된 부분을 식별하여 개선합니다.

**주요 목표**:
1. `network_service.cpp`에서 Ethernet DHCP/Static 설정 변경 감지 추가
2. `network_service.cpp`에서 WiFi 설정 변경 감지 추가
3. LoRaService 및 SwitcherService의 현재 구현 검증
4. 일관된 설정 변경 감지 패턴 수립

---

## 우선순위별 마일스톤 (Priority-Based Milestones)

### Milestone 1: 분석 및 설계 (Priority High)

**목표**: 전체 코드베이스의 설정 변경 감지 패턴 분석 완료

**작업 항목**:
- [ ] 모든 서비스의 `onConfig*` 이벤트 핸들러 검토
- [ ] 정적 `last_*` 변수 사용 패턴 문서화
- [ ] 설정 구조체(`app_network_config_t` 등)의 모든 필드 확인
- [ ] 재시작이 필요한 설정 필드 식별

**성공 기준**:
- 모든 설정 변경 감지 로직이 문서화됨
- 누락된 감지 항목 목록 작성됨

### Milestone 2: NetworkService 개선 (Priority High)

**목표**: Ethernet 및 WiFi 설정 변경 감지 완전 구현

**작업 항목**:
- [ ] `onConfigDataEvent()`에 Ethernet `dhcp_enabled` 변경 감지 추가
- [ ] `onConfigDataEvent()`에 Ethernet Static IP 관련 설정 변경 감지 추가
- [ ] `onConfigDataEvent()`에 WiFi 설정 변경 감지 추가
- [ ] 정적 변수 초기화 상태 관리 개선
- [ ] 로그 메시지 개선 (어떤 설정이 변경되었는지 명시)

**성공 기준**:
- DHCP/Static 모드 전환 시 네트워크 재시작됨
- Static IP 변경 시 네트워크 재시작됨
- WiFi SSID/Password 변경 시 WiFi 재시작됨

### Milestone 3: LoRaService 검증 (Priority Medium)

**목표**: LoRaService RF 설정 변경 감지 로직 검증

**작업 항목**:
- [ ] `on_rf_changed()` 이벤트 핸들러 동작 검증
- [ ] TX 모드에서 broadcast 로직 확인
- [ ] RX 모드에서 드라이버 적용 로직 확인
- [ ] 초기화 상태 플래그(`s_rf_initialized`) 동작 확인

**성공 기준**:
- 주파수 변경 시 드라이버에 올바르게 적용됨
- 부팅 시 불필요한 broadcast가 발생하지 않음

### Milestone 4: SwitcherService 검증 (Priority Medium)

**목표**: SwitcherService 설정 변경 감지 로직 검증

**작업 항목**:
- [ ] `checkConfigAndReconnect()` 메서드 동작 검증
- [ ] 듀얼 모드, 오프셋 변경 감지 확인
- [ ] Primary/Secondary 스위처 설정 변경 감지 확인
- [ ] Interface 변경 시 올바른 로컬 바인딩 IP 적용 확인

**성공 기준**:
- IP/Port 변경 시 스위처 재연결됨
- Interface 변경 시 올바른 네트워크 인터페이스 사용됨
- camera_limit 변경은 재연결 없이 적용됨

### Milestone 5: 코드 리팩토링 (Priority Low)

**목표**: 일관된 설정 변경 감지 패턴 적용

**작업 항목**:
- [ ] 설정 변경 감지를 위한 헬퍼 함수/매크로 작성
- [ ] 중복 코드 제거
- [ ] 테스트 가능한 구조로 리팩토링
- [ ] 문서화 업데이트

**성공 기준**:
- 코드 중복이 50% 이상 감소
- 새로운 설정 추가 시 패턴을 쉽게 적용 가능
- 유닛 테스트 작성 가능

---

## 기술 접근 방식 (Technical Approach)

### 설정 변경 감지 패턴

**기본 패턴**:

```cpp
// 1. 정적 변수로 이전 상태 저장
static bool last_<field>_enabled = false;
static char last_<field>_value[SIZE] = "";

// 2. 변경 감지
bool <field>_changed = (last_<field> != current_config.<field>);

// 3. 상태 업데이트
last_<field> = current_config.<field>;

// 4. 변경 시 액션
if (<field>_changed) {
    restart<Driver>();
}
```

**복합 조건 패턴**:

```cpp
// 여러 설정 중 하나라도 변경되면 재시작
bool need_restart = false;
need_restart |= (last_enabled != current_config.enabled);
need_restart |= (strcmp(last_ip, current_config.ip) != 0);
need_restart |= (last_port != current_config.port);

if (need_restart) {
    restart();
}
```

### 이니셜라이제이션 상태 관리

**문제**: 드라이버 초기화 전에 설정 변경 이벤트가 도착할 수 있음

**해결**: 초기화 상태 플래그 사용

```cpp
static bool s_initialized = false;

esp_err_t onConfigDataEvent(const event_data_t* event) {
    if (!s_initialized) {
        // 초기화 로직 수행
        initializeDriver();
        s_initialized = true;
    } else {
        // 변경 감지 로직 수행
        checkAndRestart();
    }
}
```

---

## 아키텍처 설계 방향 (Architecture Design Direction)

### 계층 구조

```
┌─────────────────────────────────────────┐
│         ConfigService                   │
│  (EVT_CONFIG_DATA_CHANGED 발행)          │
└─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         Event Bus                       │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        ▼                       ▼
┌──────────────┐      ┌──────────────┐
│ NetworkService│      │SwitcherService│
│  - Ethernet   │      │  - Primary   │
│  - WiFi       │      │  - Secondary │
└──────────────┘      └──────────────┘
        │
        ▼
┌──────────────┐
│ LoRaService  │
│  - RF Config │
└──────────────┘
```

### 설정 변경 감지 책임 분리

1. **ConfigService**: 설정 저장 및 이벤트 발행만 담당
2. **각 Service**: 자신에게 필요한 설정 변경만 감지하고 처리
3. **Driver**: 실제 하드웨어 재초기화 담당

---

## 위험 및 대응 계획 (Risks and Response Plans)

### Risk 1: 네트워크 재시작 빈도 과다

**위험**: 설정 변경이 너무 자주 감지되어 네트워크가 불안정해질 수 있음

**완화 방책**:
- 변경 감지 전에 실제 값 변경 확인 (strcmp 사용)
- 불필요한 재시작 방지를 위한 디바운스 로직 고려
- 로그를 통해 재시작 원인 명확히 표시

### Risk 2: 초기화 순서 문제

**위험**: 드라이버 초기화 전에 설정 변경 이벤트가 도착할 수 있음

**완화 방책**:
- `s_initialized` 플래그 사용
- 초기화 로직과 변경 감지 로직 분리
- 첫 설정 변경은 초기화로 처리

### Risk 3: 정적 변수 초기화 문제

**위험**: 정적 변수가 초기화되지 않은 상태로 사용될 수 있음

**완화 방책**:
- 명시적 초기화 (`= false`, `= ""`)
- 첫 이벤트 수신 시 현재 값으로 초기화
- 초기화 상태 추적 플래그 사용

### Risk 4: 문자열 비교 오버헤드

**위험**: 매 이벤트마다 문자열 비교가 수행되어 성능 저하 가능

**완화 방책**:
- 문자열 비교는 memcmp 사용으로 최적화 가능
- 변경 감지는 이벤트 기반이므로 빈도가 낮음
- 실제 성능 측정 후 최적화 결정

---

## 테스트 전략 (Testing Strategy)

### 유닛 테스트

**대상**:
- `NetworkServiceClass::onConfigDataEvent()`
- 설정 변경 감지 로직

**테스트 케이스**:
1. Ethernet enabled 변경 시 재시작 확인
2. Ethernet dhcp_enabled 변경 시 재시작 확인
3. Ethernet static_ip 변경 시 재시작 확인
4. WiFi SSID 변경 시 재시작 확인
5. 설정 변경 없을 시 재시작 안 함 확인

### 통합 테스트

**시나리오**:
1. 웹 인터페이스에서 DHCP → Static 전환
2. Static IP 주소 변경
3. WiFi SSID/Password 변경
4. LoRa 주파수 변경
5. Switcher IP 변경

**검증 방법**:
- 로그 메시지 확인
- 네트워크 연결 상태 확인
- 실제 통신 동작 확인

### 수동 테스트 절차

1. 기기 부팅 및 WiFi 연결
2. 웹 인터페이스 접속
3. Network 설정 페이지에서 다음 변경 수행:
   - Ethernet: Disable → Enable
   - Ethernet: DHCP → Static (IP 입력)
   - Ethernet: Static IP 변경
4. 각 변경 후 네트워크 재연결 확인
5. 로그에서 "config changed, restarting" 메시지 확인

---

## 의존성 (Dependencies)

### 내부 의존성
- `event_bus`: 이벤트 구독/발행
- `ethernet_driver`: Ethernet 재시작
- `wifi_driver`: WiFi 재시작
- `lora_driver`: LoRa 설정 적용
- `NVSConfig`: 설정 저장소

### 외부 의존성
- ESP-IDF FreeRTOS
- ESP-IDF NVS API

---

## 성공 기준 (Success Criteria)

### 기능적 요구사항
- [ ] Ethernet `dhcp_enabled` 변경 시 네트워크 재시작
- [ ] Ethernet Static IP 설정 변경 시 네트워크 재시작
- [ ] WiFi SSID/Password 변경 시 WiFi 재시작
- [ ] LoRa 주파수 변경 시 드라이버 설정 적용
- [ ] Switcher 설정 변경 시 재연결

### 비기능적 요구사항
- [ ] 설정 변경 감지 로직이 일관된 패턴을 따름
- [ ] 불필요한 재시작이 발생하지 않음
- [ ] 로그 메시지가 명확함
- [ ] 코드 중복이 최소화됨

### 품질 기준
- [ ] TRUST 5 프레임워크 준수
  - **Tested**: 85% 이상 코드 커버리지
  - **Readable**: 명확한 변수명, 주석
  - **Unified**: 일관된 코드 스타일
  - **Secured**: 입력값 검증
  - **Trackable**: Git 커밋 메시지

---

## 다음 단계 (Next Steps)

1. `/moai:2-run SPEC-CONFIG-RESTART-001` 실행하여 구현 시작
2. Milestone 순서대로 구현 진행
3. 각 Milestone 완료 후 테스트 수행
4. 모든 작업 완료 후 `/moai:3-sync SPEC-CONFIG-RESTART-001` 실행하여 문서화

---

## 참고 자료 (References)

- `spec.md`: 상세 요구사항 및 EARS 명세
- `components/03_service/network_service/network_service.cpp`
- `components/03_service/lora_service/lora_service.cpp`
- `components/03_service/switcher_service/switcher_service.cpp`
- `components/03_service/config_service/config_service.cpp`
