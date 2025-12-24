# 실시간 스위처 통신 구현 계획

> **임시 문서** - 실시간 Tally 반영을 위한 아키텍처 검토

## 목차

1. [현재 상황](#현재-상황)
2. [실시간 요구사항](#실시간-요구사항)
3. [구현 옵션 비교](#구현-옵션-비교)
4. [권장 방안](#권장-방안)
5. [구현 계획](#구현-계획)

---

## 현재 상황

### /home/prod/tally-node (현재 작업 중)

```
01_app/tally_tx_app/
└── tally_tx_app_loop()
    └── switcher_service_loop()
        └── AtemDriver::loop()
            └── recvfrom() (non-blocking, polling)
```

**문제점:**
- 메인 루프 주기에 따른 수신 지연
- 메인 루프가 다른 작업을 하면 패킷 처리 지연

### /home/dev/esp-idf (examples/2)

```
2_application/task/
└── Task.cpp
    └── createSwitcherTask()
        └── switcherTaskFunction()
            └── switcher_service_->loop() (10ms 주기)
```

**이미 구현됨!** 별도 FreeRTOS 태스크로 10ms 주기 실행.

---

## 실시간 요구사항

### ATEM 프로토콜 특성

| 항목 | 값 |
|------|-----|
| 패킷 간격 | ~10ms (최대 100Hz) |
| Keepalive | 1초 간격 |
| Tally 변경 | 사용자 조작 시 (불규칙) |
| 타임아웃 | 5초 무응답 시 연결 해제 |

### 지연 요구사항

| 용도 | 허용 지연 |
|------|----------|
| Tally 표시 (LED/OLED) | 50~100ms |
| LoRa 송신 | 100~200ms |
| **즉시 반영** | **< 20ms** |

---

## 구현 옵션 비교

### 옵션 1: 별도 FreeRTOS 태스크 (Polling)

```
┌─────────────────────────────────────────┐
│  FreeRTOS Task (Priority: 4)            │
│  ┌───────────────────────────────────┐  │
│  │  while (true) {                   │  │
│  │      switcher_service_->loop();   │  │
│  │      vTaskDelay(10);              │  │
│  │  }                                │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

| 장점 | 단점 |
|------|------|
| 구현 simple | 최대 10ms 지연 |
| 안정적 | CPU 항상 할당 |
| 이미 검증됨 (examples/2) | 유휴 시에도 리소스 사용 |
| 디버깅 용이 |  |

**구현 위치:** `03_service/switcher_service/`

```cpp
class SwitcherService {
private:
    TaskHandle_t task_handle_;
    static void taskFunction(void* param);

public:
    bool start();   // 태스크 생성
    void stop();    // 태스크 삭제
};
```

---

### 옵션 2: lwIP 소켓 콜백 (Event-driven)

```cpp
// lwIP 콜백 등록
void AtemDriver::setupSocketCallback() {
    udp_recv(pcb_, socket_callback, this);
}

// 패킷 도착 시 즉시 호출
void socket_callback(void* arg, struct udp_pcb* pcb,
                     struct pbuf* p, const ip_addr_t* addr,
                     u16_t port) {
    AtemDriver* driver = static_cast<AtemDriver*>(arg);
    driver->processPacket(p->payload, p->len);  // 즉시 처리
    pbuf_free(p);
}
```

| 장점 | 단점 |
|------|------|
| 패킷 도착 즉시 처리 (0ms 지연) | lwIP 내부에서 호출 → 스택 제약 |
| CPU 효율적 (유휴 시 0%) | 구현 복잡 |
| 진정한 실시간 | 디버깅 어려움 |
|  | lwPbuf 메모리 관리 필요 |

**구현 위치:** `04_driver/switcher_driver/atem/`

---

### 옵션 3: 하이브리드 (태스크 + 큐)

```
┌─────────────────────────────────────────────────────────────────┐
│  lwIP Callback (ISR context)                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  socket_callback(...) {                                   │  │
│  │      xQueueSendFromISR(packet_queue_, &packet, 0);        │  │
│  │  }                                                        │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  FreeRTOS Task                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  while (true) {                                           │  │
│  │      if (xQueueReceive(packet_queue_, &pkt, 10)) {        │  │
│  │          processPacket(&pkt);                             │  │
│  │      }                                                     │  │
│  │  }                                                        │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

| 장점 | 단점 |
|------|------|
| 즉시 수신 + 안정 처리 | 복잡도 증가 |
| ISR에서 손실 없이 수신 | 큐 관리 오버헤드 |
| 우선순위 제어 가능 | 메모리 사용 증가 |

---

## 권장 방안

### 단계별 접근

```
┌─────────────────────────────────────────────────────────────────┐
│  Phase 1: 별도 태스크 (즉시 구현)                                │
│  └── 10ms polling, 충분한 실시간성                              │
├─────────────────────────────────────────────────────────────────┤
│  Phase 2: Event Bus 연결 (병렬)                                 │
│  └── Tally 변경 시 즉시 broadcast                               │
├─────────────────────────────────────────────────────────────────┤
│  Phase 3: 최적화 (필요 시)                                      │
│  └── lwIP 콜백 또는 하이브리드                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 1: 별도 태스크 구현

**구현 위치:** `03_service/switcher_service/`

```cpp
// SwitcherService.h
class SwitcherService {
private:
    TaskHandle_t task_handle_;
    bool task_running_;

    static void taskFunction(void* param);
    void taskLoop();

public:
    bool start();   // 태스크 생성 및 시작
    void stop();    // 태스크 정지 및 삭제
    bool isRunning() const;
};

// SwitcherService.cpp
bool SwitcherService::start() {
    if (task_running_) {
        return true;
    }

    BaseType_t ret = xTaskCreate(
        taskFunction,            // 태스크 함수
        "switcher_svc",          // 이름
        4096,                    // 스택 크기
        this,                    // 파라미터
        5,                       // 우선순위 (높음)
        &task_handle_
    );

    task_running_ = (ret == pdPASS);
    return task_running_;
}

void SwitcherService::taskFunction(void* param) {
    SwitcherService* service = static_cast<SwitcherService*>(param);

    while (service->task_running_) {
        service->taskLoop();     // Primary + Secondary loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

void SwitcherService::taskLoop() {
    // Primary 처리
    if (primary_.adapter) {
        primary_.adapter->loop();
        checkSwitcherChange(SWITCHER_ROLE_PRIMARY);
    }

    // Secondary 처리
    if (secondary_.adapter) {
        secondary_.adapter->loop();
        checkSwitcherChange(SWITCHER_ROLE_SECONDARY);
    }
}
```

**API 변경:**

```cpp
// 변경 전
tally_tx_app_init();
tally_tx_app_start();
while (1) {
    tally_tx_app_loop();  // 내부에서 switcher_service_loop() 호출
    vTaskDelay(10);
}

// 변경 후
tally_tx_app_init();       // 내부에서 switcher_service_start() 호출
// loop()에서 switcher_service_loop() 호출 제거
while (1) {
    // 주기적 송신만 처리
    vTaskDelay(1000);
}
```

---

## 구현 계획

### 태스크 설정

| 항목 | 값 |
|------|-----|
| 이름 | `switcher_svc` |
| 스택 크기 | 4KB |
| 우선순위 | 5 (메인보다 높게) |
| 주기 | 10ms |

### Event Bus 연결

```cpp
// Tally 변경 시 즉시 발행
void SwitcherService::onSwitcherTallyChange(switcher_role_t role) {
    // 기존 콜백
    if (tally_callback_) {
        tally_callback_();
    }

    // Event Bus 발행
    event_bus_publish(EVT_TALLY_STATE_CHANGED, &role, sizeof(role));
}
```

### 파일 변경 목록

```
components/03_service/switcher_service/
├── include/SwitcherService.h
│   └── 태스크 관련 메서드 추가
└── SwitcherService.cpp
    ├── start() 구현
    ├── stop() 구현
    ├── taskFunction() 구현
    └── taskLoop() 구현

components/01_app/tally_tx_app/
└── tally_tx_app.cpp
    └── loop()에서 switcher_service_loop() 호출 제거
```

---

## 참고: examples/2 구현

```cpp
// /home/dev/esp-idf/components/2_application/task/src/Task.cpp
void Task::switcherTaskFunction(void* parameter) {
    Task* service = static_cast<Task*>(parameter);

    T_LOG_0(T_LOG_SWITCHER, "Switcher Loop 태스크 시작 (10ms 주기)");

    while (true) {
        service->switcher_service_->loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool Task::createSwitcherTask(Switcher* switcher_service) {
    switcher_task_id_ = createTask("switcher_loop", [this]() {
        switcherTaskFunction(this);
    }, 4096, 4);  // 스택 4KB, 우선순위 4

    return (switcher_task_id_ != 0);
}
```

이를 참고하여 `/home/prod/tally-node`에 동일하게 구현.

---

## 결정사항

1. **구현 방식:** 별도 FreeRTOS 태스크 (10ms polling)
2. **구현 위치:** `03_service/switcher_service/`
3. **우선순위:** 5 (높음)
4. **Event Bus:** 병렬 구현

---

*작성일: 2024-12-24*
*상태: 임시 문서 - 구현 후 삭제 또는 정식 문서로 승격
