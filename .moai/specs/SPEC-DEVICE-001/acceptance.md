# SPEC-DEVICE-001: 인수 기준

## 개요

이 문서는 SPEC-DEVICE-001의 구현 완료 여부를 검증하기 위한 인수 기준을 정의합니다.

---

## 정의 (Definition)

| 용어 | 정의 |
|------|------|
| **기능정지 상태 (Stopped State)** | RX 장치가 디스플레이와 LED를 끄고 Tally 표시를 중단한 상태 |
| **복구 (Recovery)** | 기능정지 상태에서 정상 상태로 돌아와 디스플레이/LED가 다시 켜짐 |
| **상태 요청 (0xE0)** | TX가 Broadcast로 모든 RX 장치에게 현재 상태를 요청하는 명령 |
| **상태 응답 (0xD0)** | RX가 자신의 상태(배터리, 카메라 ID, stopped 등)를 TX로 응답 |

---

## Given-When-Then 테스트 시나리오

### Scenario 1: 기능정지 상태 RX 자동 복구

**Given**:
- RX 장치가 기능정지 상태임 (`s_rx.stopped == true`)
- TX가 주기적으로 상태 요청(0xE0)을 Broadcast로 송신 중
- RX의 디스플레이와 LED가 꺼져 있음

**When**:
- TX가 상태 요청(0xE0)을 송신
- RX가 상태 요청을 수신

**Then**:
- RX가 `s_rx.stopped`를 `false`로 변경
- `EVT_STOP_CHANGED(false)` 이벤트가 발행됨
- 디스플레이와 LED가 다시 켜짐
- RX가 상태 응답(0xD0)을 송신 (`stopped = 0`)
- TX가 `device_info.is_stopped`를 `false`로 업데이트

---

### Scenario 2: 정상 상태 RX는 변경 없음

**Given**:
- RX 장치가 정상 상태임 (`s_rx.stopped == false`)
- TX가 상태 요청(0xE0)을 송신

**When**:
- RX가 상태 요청을 수신

**Then**:
- `s_rx.stopped` 값이 변경되지 않음 (`false` 유지)
- 불필요한 이벤트 발행 없음
- 상태 응답에 `stopped = 0` 포함

---

### Scenario 3: TX가 기능정지 상태 감지 로깅

**Given**:
- RX 장치가 기능정지 상태에서 상태 응답(0xD0, `stopped = 1`)을 송신
- TX가 상태 응답을 수신

**When**:
- TX의 `on_status_response()` 함수가 호출됨

**Then**:
- 로그에 "device in stopped state: ID=XXXX (will auto-recover...)" 메시지 출력
- `device_info.is_stopped`가 `true`로 설정됨
- 다음 주기 상태 요청 시 자동 복구 대기

---

### Scenario 4: 복구 후 상태 동기화

**Given**:
- RX가 기능정지 상태였다가 복구됨
- RX가 상태 응답(0xD0, `stopped = 0`)을 송신

**When**:
- TX가 복구된 상태 응답을 수신

**Then**:
- `device_info.is_stopped`가 `false`로 업데이트됨
- 디바이스 리스트 이벤트(`EVT_DEVICE_LIST_CHANGED`)에 변경된 상태 반영
- 웹 UI에서 정상 상태로 표시됨

---

### Scenario 5: 다중 RX 장치 복구

**Given**:
- 3개의 RX 장치가 모두 기능정지 상태
- TX가 상태 요청(0xE0)을 Broadcast로 송신

**When**:
- 모든 RX가 상태 요청을 수신 (랜덤 지연으로 순차적 응답)

**Then**:
- 각 RX가 독립적으로 복구 로직 실행
- 랜덤 지연(0-1000ms)으로 충돌 방지
- 모든 RX가 순차적으로 정상 상태로 복구
- TX가 모든 장치의 `is_stopped`를 `false`로 업데이트

---

## 품질 게이트 (Quality Gates)

### QG-1: 기능 완결성
- [ ] 기능정지 상태 RX가 상태 요청 수신 시 자동 복구
- [ ] 정상 상태 RX는 상태 요청 시 변경 없음
- [ ] 복구 시 디스플레이/LED 정상 활성화
- [ ] TX가 기능정지 상태를 올바르게 감지

### QG-2: 데이터 무결성
- [ ] 상태 응답의 `stopped` 필드가 올바르게 설정됨
- [ ] `device_info.is_stopped`가 상태 응답과 동기화됨
- [ ] 복구 후 `stopped = 0` 상태가 TX에 전달됨

### QG-3: 로깅 및 디버깅
- [ ] 기능정지 상태 감지 시 로그 출력
- [ ] 복구 동작 시 로그 출력
- [ ] 장치 ID가 로그에 포함되어 추적 가능

### QG-4: 기술 안정성
- [ ] 기존 코드와의 호환성 유지
- [ ] 다중 RX 환경에서 충돌 없이 동작
- [ ] 메모리 누수 없음
- [ ] 컴파일 경고 없음

---

## 검증 방법

### 수동 테스트 절차

#### 1단계: 기능정지 상태 생성

1. TX 모드 Tally Node와 RX 모드 Tally Node 준비
2. 웹 UI 또는 API를 통해 RX 장치에 기능정지 명령(0xE4) 송신
3. RX의 디스플레이와 LED가 꺼지는지 확인

#### 2단계: 복구 동작 확인

1. TX에서 주기적 상태 요청(0xE0) 대기 (최대 30초)
2. RX가 상태 요청을 수신하면 자동으로 복구되는지 확인
3. 디스플레이와 LED가 다시 켜지는지 확인
4. 시리얼 모니터에서 "auto-recovering from stopped state" 로그 확인

#### 3단계: TX 상태 동기화 확인

1. 웹 UI의 Devices 페이지 접속
2. 해당 장치의 상태가 "Stopped"에서 "Online"으로 변경되는지 확인
3. 장치 정보가 올바르게 표시되는지 확인

### 자동화 가능 테스트 (향후)

```cpp
// 테스트 가이드 (수동 검증용)
// device_manager_test.cpp

void test_stopped_state_recovery() {
    // 1. RX 상태를 기능정지로 설정
    s_rx.stopped = true;
    TEST_ASSERT_TRUE(s_rx.stopped);

    // 2. 상태 요청 패킷 생성
    lora_packet_event_t packet = {
        .data = {LORA_HDR_STATUS_REQ},
        .length = 1,
        .rssi = -50,
        .snr = 10.0f
    };

    // 3. handle_status_request 호출
    handle_status_request(&packet);

    // 4. 복구 확인
    TEST_ASSERT_FALSE(s_rx.stopped);
    // 5. 이벤트 발행 확인 (모의 객체 필요)
}
```

---

## Definition of Done

다음 조건이 모두 충족될 때 SPEC-DEVICE-001이 완료된 것으로 간주합니다.

### 1. 구현 완료
- [ ] `handle_status_request()` 함수에 복구 로직 구현
- [ ] `on_status_response()` 함수에 로깅 추가 (선택)
- [ ] 컴파일 성공 및 경고 없음

### 2. 테스트 통과
- [ ] 모든 Given-When-Then 시나리오 통과
- [ ] 모든 품질 게이트 항목 충족
- [ ] 기능정지 상태에서 자동 복구 확인

### 3. 코드 품질
- [ ] 기존 코드 스타일과 일관성 유지
- [ ] 주석 추가 (필요시)
- [ ] Magic number 제거 (매크로 활용)

### 4. 문서화
- [ ] SPEC 문서 완료
- [ ] 구현 변경사항 기록
- [ ] 코드 리뷰 완료

---

## 부록: 관련 코드 위치

### RX 모드 복구 로직
- **파일**: `components/03_service/device_manager/device_manager.cpp`
- **함수**: `handle_status_request()`
- **라인**: 약 1044-1054 (수정 전)

### TX 모드 로깅
- **파일**: `components/03_service/device_manager/device_manager.cpp`
- **함수**: `on_status_response()`
- **라인**: 약 263-340

### 프로토콜 정의
- **파일**: `components/00_common/lora_protocol/include/lora_protocol.h`
- **구조체**: `lora_msg_status_t`
- **라인**: 약 100-110

### 이벤트 정의
- **파일**: `components/00_common/event_bus/include/event_bus.h`
- **구조체**: `device_info_t`
- **이벤트**: `EVT_STOP_CHANGED`
