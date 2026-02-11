# SPEC-CONFIG-RESTART-001: 인수 기준

## TAG BLOCK

```
SPEC-ID: SPEC-CONFIG-RESTART-001
TITLE: Config Change Detection and Restart Acceptance Criteria
DOMAIN: CONFIG, RESTART, NETWORK
STATUS: PLANNED
PHASE: ACCEPTANCE
ASSIGNED: TBD
CREATED: 2026-02-11
VERSION: 1.0.0
TRACEABILITY: SPEC-CONFIG-RESTART-001
```

---

## 개요 (Overview)

이 문서는 설정 변경 감지 및 재시작 기능에 대한 인수 기준을 정의합니다. 각 시나리오는 Given-When-Then 형식으로 작성되어 테스트 가능성을 보장합니다.

---

## 인수 기준 (Acceptance Criteria)

### AC-001: Ethernet DHCP/Static 모드 전환

**설명**: 사용자가 웹 인터페이스에서 Ethernet DHCP 모드와 Static 모드를 전환할 때 네트워크가 올바르게 재시작되어야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 부팅되고 Ethernet이 DHCP 모드로 연결되어 있다
And:   사용자가 웹 인터페이스에 접속하여 Network 설정 페이지에 있다
When:  사용자가 Ethernet 설정을 "Static IP" 모드로 변경하고 저장한다
And:   Static IP, Netmask, Gateway를 입력한다
Then:  시스템은 EVT_CONFIG_DATA_CHANGED 이벤트를 수신해야 한다
And:   onConfigDataEvent 핸들러는 dhcp_enabled 변경을 감지해야 한다
And:   Ethernet 드라이버는 재시작되어야 한다
And:   새로운 Static IP로 네트워크에 연결되어야 한다
And:   로그에 "Ethernet config changed, restarting..." 메시지가 출력되어야 한다
```

```gherkin
Given: 기기가 Ethernet Static IP 모드로 연결되어 있다
When:  사용자가 Ethernet 설정을 "DHCP" 모드로 변경하고 저장한다
Then:  시스템은 dhcp_enabled 변경을 감지해야 한다
And:   Ethernet 드라이버는 재시작되어야 한다
And:   DHCP 서버에서 IP를 할당받아야 한다
```

### AC-002: Ethernet Static IP 주소 변경

**설명**: 사용자가 Ethernet Static IP 주소, 서브넷 마스크, 게이트웨이를 변경할 때 네트워크가 재시작되어야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 Ethernet Static IP 모드로 연결되어 있다
And:   현재 IP가 192.168.1.100으로 설정되어 있다
When:  사용자가 Static IP를 192.168.1.101로 변경하고 저장한다
Then:  시스템은 static_ip 변경을 감지해야 한다
And:   Ethernet 드라이버는 재시작되어야 한다
And:   새로운 IP(192.168.1.101)로 네트워크에 연결되어야 한다
```

```gherkin
Given: 기기가 Ethernet Static IP 모드로 연결되어 있다
When:  사용자가 Subnet Mask를 변경하고 저장한다
Then:  시스템은 static_netmask 변경을 감지해야 한다
And:   Ethernet 드라이버는 재시작되어야 한다
```

```gherkin
Given: 기기가 Ethernet Static IP 모드로 연결되어 있다
When:  사용자는 Gateway를 변경하고 저장한다
Then:  시스템은 static_gateway 변경을 감지해야 한다
And:   Ethernet 드라이버는 재시작되어야 한다
```

### AC-003: WiFi SSID/Password 변경

**설명**: 사용자가 WiFi SSID 또는 Password를 변경할 때 WiFi가 재시작되어야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 WiFi 네트워크 "MyWiFi"에 연결되어 있다
When:  사용자가 WiFi SSID를 "NewWiFi"로 변경하고 저장한다
Then:  시스템은 ssid 변경을 감지해야 한다
And:   WiFi 드라이버는 재시작되어야 한다
And:   새로운 SSID로 연결을 시도해야 한다
```

```gherkin
Given: 기기가 WiFi 네트워크에 연결되어 있다
When:  사용자가 WiFi Password를 변경하고 저장한다
Then:  시스템은 password 변경을 감지해야 한다
And:   WiFi 드라이버는 재시작되어야 한다
And:   새로운 비밀번호로 인증을 시도해야 한다
```

### AC-004: LoRa 주파수 및 Sync Word 변경

**설명**: 사용자가 LoRa 주파수 또는 Sync Word를 변경할 때 드라이버 설정이 즉시 적용되어야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 TX 모드로 LoRa 통신 중이다
And:   현재 주파수가 915.0 MHz로 설정되어 있다
When:  사용자가 LoRa 주파수를 920.0 MHz로 변경하고 저장한다
Then:  시스템은 EVT_RF_CHANGED 이벤트를 발행해야 한다
And:   LoRaService는 frequency 변경을 감지해야 한다
And:   TX 모드: 10회 broadcast를 전송해야 한다
And:   드라이버에 새 주파수가 적용되어야 한다
```

```gherkin
Given: 기기가 RX 모드로 LoRa 수신 대기 중이다
When:  사용자가 LoRa Sync Word를 0x12에서 0x34로 변경한다
Then:  LoRaService는 sync_word 변경을 감지해야 한다
And:   드라이버에 새 sync word가 즉시 적용되어야 한다
And:   broadcast는 전송되지 않아야 한다 (RX 모드)
```

### AC-005: Switcher IP 및 Port 변경

**설명**: 사용자가 Switcher IP 또는 Port를 변경할 때 스위처 연결이 재설정되어야 한다.

**Given-When-Then**:

```gherkin
Given: Primary Switcher가 192.168.1.50:9910에 연결되어 있다
When:  사용자가 Primary Switcher IP를 192.168.1.51로 변경한다
Then:  SwitcherService는 ip_changed를 감지해야 한다
And:   기존 연결이 해제되어야 한다
And:   새로운 IP로 연결을 시도해야 한다
```

```gherkin
Given: Primary Switcher가 연결되어 있다
When:  사용자가 Primary Switcher Port를 9910에서 9911로 변경한다
Then:  SwitcherService는 port_changed를 감지해야 한다
And:   스위처 어댑터가 재설정되어야 한다
And:   새로운 Port로 연결을 시도해야 한다
```

### AC-006: Switcher Network Interface 변경

**설명**: 사용자가 Switcher Network Interface를 변경할 때 올바른 로컬 바인딩 IP가 사용되어야 한다.

**Given-When-Then**:

```gherkin
Given: Primary Switcher가 "Auto" 인터페이스로 연결되어 있다
And:   Ethernet IP가 192.168.1.100, WiFi STA IP가 192.168.2.100이다
When:  사용자가 Primary Switcher Interface를 "Ethernet"으로 변경한다
Then:  SwitcherService는 interface_changed를 감지해야 한다
And:   AtemDriver는 로컬 바인딩 IP로 192.168.1.100을 사용해야 한다
And:   스위처가 재연결되어야 한다
```

```gherkin
Given: Primary Switcher가 "Ethernet" 인터페이스로 연결되어 있다
When:  사용자가 Primary Switcher Interface를 "WiFi"으로 변경한다
Then:  SwitcherService는 interface_changed를 감지해야 한다
And:   AtemDriver는 로컬 바인딩 IP로 WiFi STA IP를 사용해야 한다
And:   스위처가 재연결되어야 한다
```

### AC-007: 불필요한 재시작 방지

**설명**: 설정이 실제로 변경되지 않았을 때는 재시작이 발생하지 않아야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 Ethernet Static IP 모드로 192.168.1.100에 연결되어 있다
When:  사용자가 설정 페이지를 열고 동일한 값(변경 없음)으로 저장한다
Then:  시스템은 설정 변경이 없음을 감지해야 한다
And:   Ethernet 드라이버는 재시작되지 않아야 한다
And:   네트워크 연결이 유지되어야 한다
```

```gherkin
Given: 기기가正常运行 중이다
When:  EVT_CONFIG_DATA_CHANGED 이벤트가 수신되지만 모든 값이 동일하다
Then:  어떠한 드라이버도 재시작되지 않아야 한다
And:   로그에 "config changed" 메시지가 출력되지 않아야 한다
```

### AC-008: 초기화 상태에서의 설정 변경

**설명**: 드라이버가 초기화되지 않은 상태에서 설정 변경 이벤트가 수신되면 초기화가 수행되어야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 부팅되었지만 드라이버가 아직 초기화되지 않았다
When:  EVT_CONFIG_DATA_CHANGED 이벤트가 수신된다
Then:  onConfigDataEvent 핸들러는 초기화 로직을 수행해야 한다
And:  드라이버가 초기화되어야 한다
And:  현재 설정값으로 드라이버가 시작되어야 한다
```

### AC-009: 로그 메시지 명확성

**설명**: 설정 변경 시 로그 메시지가 어떤 설정이 변경되었는지 명확히 표시해야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가正常运行 중이다
When:  Ethernet dhcp_enabled 설정이 변경된다
Then:  로그에 "Ethernet config changed, restarting..." 메시지가 출력되어야 한다
And:  변경된 필드(예: dhcp_enabled)가 로그에 표시되어야 한다 (권장)
```

### AC-010: LoRa 부팅 시 Broadcast 방지

**설명**: 부팅 시 첫 RF 설정 변경으로 인해 불필요한 broadcast가 발생하지 않아야 한다.

**Given-When-Then**:

```gherkin
Given: 기기가 부팅되어 TX 모드로 초기화 중이다
When:  첫 번째 EVT_RF_CHANGED 이벤트가 수신된다
Then:  s_rf_initialized 플래그가 false여야 한다
And:  broadcast는 전송되지 않아야 한다
And:  s_rf_initialized가 true로 설정되어야 한다
And:  드라이버에 RF 설정이 적용되어야 한다
```

---

## 품질 게이트 (Quality Gates)

### TRUST 5 기준

**Tested**:
- [ ] 모든 AC에 해당하는 테스트 케이스 작성
- [ ] 유닛 테스트 커버리지 85% 이상
- [ ] 통합 테스트 통과

**Readable**:
- [ ] 명확한 변수명 (`last_eth_dhcp_enabled`)
- [ ] 주석으로 변경 감지 로직 설명
- [ ] 일관된 코드 스타일

**Unified**:
- [ ] 모든 서비스에 동일한 패턴 적용
- [ ] 코드 포맷팅 도구 통과
- [ ] 린터 경고 0개

**Secured**:
- [ ] 문자열 복사 시 버퍼 오버플로우 방지
- [ ] 입력값 검증 수행
- [ ] 정적 변수 초기화 확인

**Trackable**:
- [ ] Git 커밋 메시지에 SPEC ID 참조
- [ ] 각 변경 사항에 대해 로그 메시지 존재
- [ ] 이슈 트래킹 시스템 연동

### 성능 기준

- [ ] 설정 변경 감지 처리 시간 < 100ms
- [ ] 네트워크 재시작 시간 < 5초
- [ ] 메모리 사용량 증가 < 1KB

### 호환성 기준

- [ ] 기존 NVS 설정 구조와 호환
- [ ] 기존 웹 인터페이스와 호환
- [ ] 다른 서비스에 영향 없음

---

## 테스트 시나리오 (Test Scenarios)

### 시나리오 1: DHCP → Static 전환

1. 기기 부팅 및 WiFi 연결
2. 웹 인터페이스 접속
3. Network 설정 페이지 이동
4. Ethernet 설정: DHCP → Static 선택
5. Static IP: 192.168.1.100 입력
6. Netmask: 255.255.255.0 입력
7. Gateway: 192.168.1.1 입력
8. 저장 버튼 클릭
9. **검증**: 로그에 "Ethernet config changed" 메시지 확인
10. **검증**: 네트워크 재연결 확인
11. **검증**: 192.168.1.100로 ping 성공

### 시나리오 2: Static IP 변경

1. 기기가 Static IP 192.168.1.100로 연결됨
2. 웹 인터페이스 접속
3. Network 설정 페이지 이동
4. Static IP를 192.168.1.101로 변경
5. 저장 버튼 클릭
6. **검증**: 로그에 "static_ip changed" 메시지 확인
7. **검증**: 192.168.1.101로 ping 성공

### 시나리오 3: WiFi SSID 변경

1. 기기가 WiFi "OldSSID"에 연결됨
2. 웹 인터페이스 접속
3. Network 설정 페이지 이동
4. WiFi SSID를 "NewSSID"로 변경
5. 저장 버튼 클릭
6. **검증**: 로그에 "WiFi config changed" 메시지 확인
7. **검증**: "NewSSID"에 연결됨

### 시나리오 4: LoRa 주파수 변경

1. 기기가 TX 모드로 LoRa 915.0 MHz 사용 중
2. 웹 인터페이스 접속
3. LoRa 설정 페이지 이동
4. 주파수를 920.0 MHz로 변경
5. 저장 버튼 클릭
6. **검증**: 로그에 "RF broadcast start" 메시지 확인 (TX 모드)
7. **검증**: RX 기기에서 920.0 MHz로 수신 가능

### 시나리오 5: Switcher IP 변경

1. 기기가 Primary Switcher 192.168.1.50에 연결됨
2. 웹 인터페이스 접속
3. Switcher 설정 페이지 이동
4. Primary IP를 192.168.1.51로 변경
5. 저장 버튼 클릭
6. **검증**: 로그에 "Primary switcher config changed" 메시지 확인
7. **검증**: 192.168.1.51에 Tally 연결 확인

---

## Definition of Done

각 Milestone은 다음 조건을 모두 만족해야 "완료"로 간주됩니다:

1. **구현 완료**: 해당 Milestone의 모든 작업 항목 구현
2. **테스트 통과**: 해당 AC에 대한 테스트 통과
3. **코드 리뷰**: 팀 리뷰 완료 및 승인
4. **문서화**: 코드 주석 및 문서 업데이트
5. **커밋**: Git 커밋 완료 (SPEC-ID 참조)
6. **로그 검증**: 실제 기기에서 로그 확인

---

## 검증 방법 (Verification Methods)

### 자동화된 테스트
- 유닛 테스트 (Google Test Framework)
- 모의 객체 (Mock)를 사용한 이벤트 핸들러 테스트

### 수동 테스트
- 실제 기기에서 웹 인터페이스를 통한 설정 변경
- 시리얼 콘솔을 통한 로그 확인
- 네트워크 유틸리티(ping, netcat)를 통한 연결 확인

### 정적 분석
- cppcheck를 통한 정적 코드 분석
- clang-tidy를 통한 코드 스타일 확인

---

## 참고 자료 (References)

- `spec.md`: 상세 요구사항
- `plan.md`: 구현 계획
- `components/03_service/`: 관련 서비스 구현
