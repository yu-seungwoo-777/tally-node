# ATEM 좀비 연결 문제 분석 및 해결

## 문제 개요

**날짜**: 2026-02-20
**심각도**: 높음 (High)
**영향**: ATEM 스위처와의 연결이 "connected" 상태로 표시되지만 실제로는 PGM/PVW 데이터가 수신되지 않음

### 증상

1. 이더넷 연결은 정상 상태
2. 웹서버 접근 가능
3. ATEM 스위처 상태: "connected"로 표시됨
4. **실제 PGM/PVW**: 변화하지 않음 (좀비 상태)
5. 재연결(IP 변경) 시 일시적으로 해결되지만 다시 발생

### 발생 조건

- 장시간 사용 후 (몇 시간 ~ 하루 이상)
- 네트워크 일시적 불안정 후
- ATEM 스위처 재부팅 후

---

## 근본 원인 분석

### 1차 원인: 하트비트 응답 검증 부재

ATEM 드라이버는 1초마다 keepalive 패킷을 전송하지만, 스위처의 응답을 검증하지 않았습니다.

```cpp
// 기존 코드 (문제점)
void AtemDriver::loop() {
    // Keepalive 전송만 하고 응답 확인 없음
    if (state_.initialized && now - state_.last_keepalive_ms > 1000) {
        sendPacket(keepalive, ...);
        state_.last_keepalive_ms = now;
    }
    // 응답 검증 없음 → 좀비 연결 감지 불가
}
```

### 2차 원인: 연결 상태 비동기화

`state_.connected` 플래그는 Hello 패킷 수신 시 `true`로 설정되지만, 실제 프로토콜 활성화 상태와 별개로 동작했습니다.

```cpp
// Hello 응답 수신 시 즉시 connected = true
if (flags & ATEM_FLAG_HELLO) {
    state_.connected = true;  // 너무 일찍 설정됨
    setConnectionState(CONNECTION_STATE_CONNECTED);
}
```

### 상태 전이 문제

```
정상 상태: Hello → InCm → TlIn (지속)

좀비 상태: Hello → connected=true → TlIn 멈춤 → 연결 상태 유지 (문제!)
```

---

## 해결 방안

### 구현 내용

#### 1. AtemState 구조체 확장

`components/04_driver/switcher_driver/atem/include/atem_driver.h`

```cpp
// 하트비트 응답 추적 (좀비 연결 탐지)
uint32_t last_heartbeat_ack_ms;  ///< 마지막 하트비트 응답 시간 (ms)
uint8_t missed_heartbeats;       ///< 연속으로 놓친 하트비트 수
static constexpr uint8_t MAX_MISSED_HEARTBEATS = 3;  ///< 최대 허용 놓친 하트비트 수
static constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 5000; ///< 하트비트 타임아웃 (5초)
```

#### 2. 하트비트 타임아웃 검증

`components/04_driver/switcher_driver/atem/atem_driver.cpp` - loop()

```cpp
// 하트비트 타임아웃 검증 (좀비 연결 탐지)
if (state_.initialized && state_.last_heartbeat_ack_ms > 0) {
    uint32_t time_since_heartbeat = now - state_.last_heartbeat_ack_ms;

    // 하트비트 타임아웃 발생 (5초 이상 응답 없음)
    if (time_since_heartbeat > AtemState::HEARTBEAT_TIMEOUT_MS) {
        state_.missed_heartbeats++;

        T_LOGW(TAG, "[%s] hb:timeout:%dms missed:%d/%d",
                config_.name.c_str(),
                (int)time_since_heartbeat,
                state_.missed_heartbeats,
                AtemState::MAX_MISSED_HEARTBEATS);

        // 연속 3회 타임아웃 시 재연결
        if (state_.missed_heartbeats >= AtemState::MAX_MISSED_HEARTBEATS) {
            T_LOGE(TAG, "[%s] hb:zombie_detected:%d consecutive timeouts, reconnecting",
                    config_.name.c_str(), state_.missed_heartbeats);
            disconnect();
            return -1;
        }

        // 타임아웃 발생 시 갱신하여 중복 탐지 방지
        state_.last_heartbeat_ack_ms = now;
    }
}
```

#### 3. 패킷 수신 시 하트비트 갱신

모든 유효한 패킷 수신 시 하트비트 타이머를 리셋합니다.

```cpp
// Hello 응답 수신 시
state_.last_heartbeat_ack_ms = getMillis();
state_.missed_heartbeats = 0;

// 모든 유효한 패킷 수신 시 (processPacket)
state_.last_heartbeat_ack_ms = now;
if (state_.missed_heartbeats > 0) {
    T_LOGD(TAG, "hb:recovered:%d->0", state_.missed_heartbeats);
    state_.missed_heartbeats = 0;
}
```

#### 4. 연결 상태 확인 개선

```cpp
bool AtemDriver::isConnected() const {
    // 하트비트 응답이 너무 오래된 경우는 연결되지 않은 것으로 간주
    if (state_.connected && state_.initialized &&
        state_.last_heartbeat_ack_ms > 0) {
        uint32_t now = getMillis();
        uint32_t time_since_heartbeat = now - state_.last_heartbeat_ack_ms;

        // 하트비트 타임아웃 시간보다 길면 연결 끊김으로 간주
        if (time_since_heartbeat > AtemState::HEARTBEAT_TIMEOUT_MS) {
            return false;
        }
    }
    return state_.connected;
}
```

---

## 동작 시나리오

### 정상 동작

```
t=0s    : Hello 패킷 수신 → last_heartbeat_ack_ms = 0, missed = 0
t=1s    : TlIn 패킷 수신 → last_heartbeat_ack_ms = 1000, missed = 0
t=2s    : TlIn 패킷 수신 → last_heartbeat_ack_ms = 2000, missed = 0
...
```

### 좀비 탐지 및 복구

```
t=0s    : 마지막 패킷 수신 → last_heartbeat_ack_ms = 0
t=5s    : hb:timeout:5001ms missed:1/3 (경고)
t=10s   : hb:timeout:5002ms missed:2/3 (경고)
t=15s   : hb:timeout:5003ms missed:3/3
        : hb:zombie_detected:3 consecutive timeouts, reconnecting
        : disconnect() 호출
t=16s   : 자동 재연결 시도
t=17s   : Hello 응답 수신 → connected
```

### 복구 시나리오 (일시적 네트워크 문제)

```
t=0s    : 마지막 패킷 수신
t=5s    : hb:timeout:5001ms missed:1/3
t=8s    : TlIn 패킷 재수신 → hb:recovered:1->0
```

---

## 수정 파일

| 파일 | 변경 사항 |
|------|----------|
| `components/04_driver/switcher_driver/atem/include/atem_driver.h` | 하트비트 추적 필드 추가 |
| `components/04_driver/switcher_driver/atem/atem_driver.cpp` | 타임아웃 검증 로직 구현 |

---

## 테스트 방법

### 1. 정상 동작 테스트

- ATEM 스위처 정상 연결
- PGM/PVW 변경 시 실시간 반영 확인
- 로그에 `hb:timeout` 메시지 없어야 함

### 2. 좀비 연결 시뮬레이션

- ATEM 스위처 전원 끄기 (연결 유지 상태)
- 15초 후 자동 재연결 로그 확인
- 스위처 전원 켜기 후 자동 복구 확인

### 3. 네트워크 복구 테스트

- 일시적 네트워크 차단 (5-10초)
- `hb:recovered` 로그 확인
- PGM/PVW 동기화 복구 확인

---

## 예상 로그 출력

```
[04_Atem] hello:tx
[04_Atem] hello:rx:0x1234,0
[04_Atem] hello:ack:tx
[04_Atem] [Primary] state:CONNECTED
[04_Atem] [Primary] init complete
[04_Atem] [Primary] state:READY

// 정상 상태 (1초마다 keepalive)
[04_Atem] [Primary] ka:1000ms

// 좀비 탐지
[04_Atem] [Primary] ka:1000ms
[04_Atem] [Primary] hb:timeout:5001ms missed:1/3
[04_Atem] [Primary] ka:1000ms
[04_Atem] [Primary] hb:timeout:5002ms missed:2/3
[04_Atem] [Primary] ka:1000ms
[04_Atem] [Primary] hb:timeout:5003ms missed:3/3
[04_Atem] [Primary] hb:zombie_detected:3 consecutive timeouts, reconnecting
[04_Atem] disconnect

// 자동 재연결
[04_Atem] [Primary] hello:tx
[04_Atem] [Primary] hello:rx:0x1235,0
[04_Atem] [Primary] state:CONNECTED
```

---

## 참고 사항

### 타임아웃 값 설정

- `HEARTBEAT_TIMEOUT_MS = 5000` (5초)
- `MAX_MISSED_HEARTBEATS = 3`
- **최대 좀비 탐지 시간**: 15초

### Keepalive 간격

- 기존: 1초 (ATEM_KEEPALIVE_INTERVAL_MS)
- 변경 없음: 기존 동작 유지

---

## 개선 이력

| 버전 | 날짜 | 변경 내용 |
|------|------|----------|
| 2.4.x | 2026-02-20 | 좀비 연결 탐지 기능 추가 |
