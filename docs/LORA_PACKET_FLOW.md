# LoRa 패킷 처리 흐름

**작성일**: 2025-12-31
**버전**: 1.0

---

## 개요

TX 모드와 RX 모드에서 LoRa 패킷을 처리하는 방식을 정의합니다.

---

## 모드별 통신 방향

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           TX ↔ RX 통신                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  [TX 모드]                             [RX 모드]                            │
│  ─────────                             ─────────                            │
│                                                                             │
│  Switcher → LoRa 송신                   LoRa 수신 → Tally 표시                │
│                                                                             │
│  ─────────────────────────────────────────────────────────────────────────   │
│  [추후 계획] TX ↔ RX 양방향 통신                                             │
│  - TX → RX: 관리 명령 (0xE0~0xEF)                                           │
│  - RX → TX: 상태 응답 (0xD0~0xDF)                                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 패킷 헤더 정의

| 헤더 | 방향 | 용도 | 처리 계층 |
|------|------|------|-----------|
| **0xF1** | TX → RX | Tally 데이터 | lora_service |
| 0xE0~0xEF | TX → RX | 관리 명령 (추후) | lora_service |
| 0xD0~0xDF | RX → TX | 상태 응답 (추후) | lora_service |

---

## 현재 구현 범위

### TX 모드 (Switcher → LoRa)

```
Switcher (ATEM/OBS/vMix)
    ↓ Tally 상태 변경
switcher_service
    ↓ packed_data_t 생성
lora_service_send_tally()
    ↓ 패킷: [F1][ChannelCount][Data...]
LoRa 송신
```

### RX 모드 (LoRa → Tally 표시)

```
LoRa 수신
    ↓ DIO1 인터럽트
lora_driver (on_driver_receive)
    ↓
lora_service (패킷 분류)
    ↓ 0xF1 헤더 확인
process_tally_packet()
    ↓ packed_data_t 파싱
EVT_TALLY_STATE_CHANGED 발행
    ↓
DisplayManager / WS2812Driver
    ↓ PGM/PVW 표시, LED 색상 변경
```

---

## LoRa Service 패킷 분류 로직

### 처리 위치

- **계층**: 03_service/lora_service
- **함수**: `on_driver_receive()` → `classify_and_process_packet()`
- **분류 기준**: 패킷 헤더 (`data[0]`)

### 분류 흐름도

```
                    on_driver_receive()
                            │
                            ↓
                ┌───────────────────────┐
                │ 패킷 길이 검사         │
                └───────────────────────┘
                            │
                ┌───────────┴───────────┐
                │                       │
                ↓                       ↓
        ┌───────────────┐       ┌──────────────┐
        │ data[0] 확인  │       │   무시/로그   │
        └───────────────┘       └──────────────┘
                │
    ┌───────────┼───────────┐
    │           │           │
    ↓           ↓           ↓
0xF1~0xF4   0xE0~0xEF   0xD0~0xDF
(Tally)    (TX→RX)     (RX→TX)
    │           │           │
    │       [추후 구현]  [추후 구현]
    │
    ↓
process_tally_packet()
    │
    ↓
EVT_TALLY_STATE_CHANGED
```

---

## 패킷 구조

### Tally 패킷 (0xF1)

```
[F1][ChannelCount][Data...]
├─ F1: 고정 헤더
├─ ChannelCount: 채널 수 (1-20)
└─ Data: packed tally 데이터
    - 채널당 2비트
    - (채널 수 + 3) / 4 바이트
    - 예: 4채널=1바이트, 20채널=5바이트
```

---

## 이벤트 발행

### Tally 상태 변경 이벤트

```cpp
// event_bus.h
typedef struct __attribute__((packed)) {
    uint8_t source;          // 스위처 소스 (0=Primary, 1=Secondary)
    uint8_t channel_count;   // 채널 수 (1-20)
    uint8_t tally_data[8];   // Packed 데이터
    uint64_t tally_value;    // 64비트 packed 값
} tally_event_data_t;

// 이벤트 타입
EVT_TALLY_STATE_CHANGED
```

### 구독자

| 컴포넌트 | 처리 내용 |
|---------|-----------|
| DisplayManager | PGM/PVW 채널 표시 |
| WS2812Driver | LED 색상 변경 (PGM=빨강, PVW=초록) |

---

## 추후 확장 계획

### TX → RX 관리 명령

| 헤더 | 명령 | 용도 |
|------|------|------|
| 0xE0 | 상태 요청 | 모든 RX 상태 조회 |
| 0xE1 | 밝기 설정 | RX 밝기 변경 |
| 0xE2 | 카메라 ID 설정 | RX 카메라 ID 변경 |
| 0xE3 | RF 설정 | 주파수/SyncWord 변경 |
| 0xE4 | 기능 정지 | RX 기능 정지 |
| 0xE5 | 재부팅 | RX 재부팅 |
| 0xE6 | PING | 지연시간 측정 |

### RX → TX 상태 응답

| 헤더 | 응답 | 용도 |
|------|------|------|
| 0xD0 | 상태 정보 | 배터리, 카메라 ID, 업타임 등 |
| 0xD1 | ACK | 명령 승인 |
| 0xD2 | PONG | PING 응답 |
