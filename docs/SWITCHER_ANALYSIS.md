# 스위처 통신 기능 분석

**작성일**: 2025-12-23
**대상**: examples/2 컴포넌트
**분석 대상**: ATEM/OBS/vMix 스위처 통신 아키텍처

---

## 1. 아키텍처 개요

examples/2는 **4계층 아키텍처**를 사용합니다:

```
┌─────────────────────────────────────────────────────────┐
│ 2_application (앱 계층)                                 │
│ - TallyPacketizer: LoRa 패킷 생성                      │
│ - StatusManager: 상태 관리                             │
│ - Bootstrap: 초기화 및 부트스트랩                      │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 1_presentation (프레젠테이션 계층)                     │
│ - DisplayManager: OLED 디스플레이 관리                 │
│ - WebServer: 웹 설정 인터페이스                       │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 3_domain (도메인 계층)                                 │
│ - Switcher: 스위처 생명주기 관리                        │
│ - PackedData: 가변 길이 packed 데이터                  │
│ - SwitcherTypes: 공통 타입 정의                        │
└─────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────┐
│ 4_infrastructure (인프라 계층)                         │
│ - switcher_driver: ATEM/OBS/vMix 어댑터               │
│ - network: WiFi/Ethernet 드라이버                     │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 디렉토리 구조

```
examples/2/components/
├── 2_application/           # 앱 계층
│   ├── tally_packetizer/   # LoRa 패킷 변환
│   ├── status_manager/     # 상태 관리
│   └── bootstrap/          # 초기화
│
├── 1_presentation/         # 프레젠테이션 계층
│   ├── display/            # OLED 디스플레이
│   └── web/                # 웹 서버
│
├── 3_domain/               # 도메인 계층
│   ├── switcher/           # 스위처 관리자
│   └── ports/              # 포트 정의
│       ├── PackedData.h    # packed 데이터 구조
│       └── SwitcherTypes.h # 공통 타입
│
└── 4_infrastructure/       # 인프라 계층
    ├── switcher_driver/    # 스위처 드라이버
    │   ├── _include/
    │   │   └── ISwitcherPort.h  # 공통 인터페이스
    │   ├── atem/            # ATEM 어댑터
    │   ├── obs/             # OBS 어댑터
    │   └── vmix/            # vMix 어댑터
    └── network/            # 네트워크 드라이버
        ├── wifi/
        └── ethernet/
```

---

## 3. 핵심 인터페이스

### ISwitcherPort (공통 인터페이스)

```cpp
class ISwitcherPort {
public:
    virtual ~ISwitcherPort() = default;

    // 연결 관리
    virtual bool initialize() = 0;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual int loop() = 0;  // 루프 처리, 반환값: 에러 코드 또는 처리된 패킷 수

    // 상태 조회
    virtual ConnectionState getConnectionState() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool isInitialized() const = 0;
    virtual PackedData getPackedTally() const = 0;
    virtual uint8_t getCameraCount() const = 0;
    virtual uint32_t getLastUpdateTime() const = 0;
    virtual SwitcherType getType() const = 0;
    virtual uint32_t getConnectTimeout() const = 0;

    // 제어
    virtual void cut() = 0;
    virtual void autoTransition() = 0;
    virtual void setPreview(uint16_t source_id) = 0;

    // 콜백 설정
    virtual void setTallyCallback(std::function<void()> callback) = 0;
    virtual void setConnectionCallback(std::function<void(ConnectionState)> callback) = 0;
};
```

---

## 4. 공통 타입 정의

### SwitcherType

```cpp
enum class SwitcherType : uint8_t {
    ATEM = 0,    // Blackmagic ATEM (UDP, 포트 9910)
    OBS = 1,     // OBS Studio (WebSocket, 포트 4455)
    VMIX = 2     // vMix (TCP, 포트 8099)
};
```

### ConnectionState

```cpp
enum class ConnectionState : uint8_t {
    DISCONNECTED = 0,    // 연결 안됨
    CONNECTING = 1,      // 연결 시도 중
    CONNECTED = 2,       // 연결됨
    INITIALIZING = 3,    // 초기화 중
    READY = 4            // 사용 가능
};
```

### TallyStatus (채널별 상태)

```cpp
enum class TallyStatus : uint8_t {
    OFF = 0,        // 0b00: Off
    PROGRAM = 1,    // 0b01: Program (bit0)
    PREVIEW = 2,    // 0b10: Preview (bit1)
    BOTH = 3        // 0b11: Program + Preview
};
```

---

## 5. PackedData 구조

### 가변 길이 Packed 데이터

```cpp
struct PackedData {
    std::vector<uint8_t> data;  // packed 데이터 바이트 배열
    uint8_t channel_count;       // 실제 채널 수

    // 채널당 2비트 필요
    // 크기: (채널 수 + 3) / 4 바이트
    // 예: 4채널=1B, 8채널=2B, 12채널=3B, 20채널=5B
};
```

### 채널 패킹 방식

```
비트 구조: [채널N][채널N-1]...[채널2][채널1]
각 채널: 2비트 (00=OFF, 01=PGM, 10=PVW, 11=Both)

예시: 4채널 (1바이트)
┌─────────────────────────────────────────────────────────────┐
│ Byte 0                                                     │
├──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┤
│ b7   │ b6   │ b5   │ b4   │ b3   │ b2   │ b1   │ b0   │
├──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┤
│ CH4  │ CH4  │ CH3  │ CH3  │ CH2  │ CH2  │ CH1  │ CH1  │
│(bit1)│(bit0)│(bit1)│(bit0)│(bit1)│(bit0)│(bit1)│(bit0)│
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
```

---

## 6. ATEM 프로토콜 (UDP)

### 프로토콜 상수

| 항목 | 값 |
|------|-----|
| 포트 | 9910 (UDP) |
| Hello 패킷 | 20바이트 |
| ACK 패킷 | 12바이트 |
| Keepalive 간격 | 1초 |
| 연결 타임아웃 | 10초 |
| 무응답 타임아웃 | 5초 |
| 바이트 순서 | Big-Endian |

### 패킷 헤더 플래그

```cpp
FLAG_ACK_REQUEST = 0x01   // 수신 확인 요청
FLAG_HELLO = 0x02         // Hello 패킷
FLAG_RESEND = 0x04        // 재전송 패킷
FLAG_REQUEST_RESEND = 0x08 // 재전송 요청
FLAG_ACK = 0x10           // 수신 확인
```

### 주요 명령

| 명령 문자열 | 설명 |
|-------------|------|
| `_ver` | Protocol Version |
| `_pin` | Product ID |
| `_top` | Topology |
| `_TlC` | Tally Channel Config |
| `PrgI` | Program Input |
| `PrvI` | Preview Input |
| `TlIn` | Tally By Index |
| `InCm` | Initialization Complete |

---

## 7. Switcher 클래스 (도메인 계층)

```cpp
class Switcher {
public:
    // 다중 스위처 관리
    size_t addSwitcher(std::unique_ptr<ISwitcherPort> adapter);
    void removeSwitcher(size_t index);
    void loop();  // 모든 스위처의 loop 호출

    // Tally 데이터 조회
    PackedData getSwitcherPacked(size_t index) const;
    uint8_t getSwitcherCameraCount(size_t index) const;
    ConnectionState getSwitcherConnectionState(size_t index) const;

    // 듀얼모드 설정
    void setDualMode(bool enabled);
    void setSecondaryOffset(uint8_t offset);
};
```

---

## 8. 듀얼모드 맵핑 전략

### 맵핑 과정

```
1. 오프셋 적용 (Shift)
   secondary_data << (offset * 2)

2. 마스킹 (Channel Limit)
   masked = shifted & mask

3. 결합 (OR)
   combined = primary | secondary
```

### 예시

```
Primary ATEM:   offset=0,  limit=8  → 채널 1-8
Secondary ATEM: offset=4,  limit=6  → 채널 5-10

Primary Tally:   PGM=1, PVW=4  → 0x42
Secondary Tally: PGM=5, PVW=6  → 0x1400

1. Secondary Shift: 0x1400 << 8 = 0x140000
2. 결합: 0x42 | 0x140000 = 0x140042

결과:
- 채널 1: PGM (Primary)
- 채널 4: PVW (Primary)
- 채널 9: PGM (Secondary 5 + offset 4)
- 채널 10: PVW (Secondary 6 + offset 4)
```

---

## 9. LoRa 패킷 구조

### 헤더 기반 가변 길이 패킷

```
┌─────────┬─────────────────┐
│ Header  │ Data            │
├─────────┼─────────────────┤
│ 0xF1    │ 2B (최대 8채널) │
│ 0xF2    │ 3B (최대 12채널)│
│ 0xF3    │ 4B (최대 16채널)│
│ 0xF4    │ 5B (최대 20채널)│
└─────────┴─────────────────┘
```

### 전송 예시

```
Combined Tally: 0x14000042
최대 채널: 10 → 12채널 패킷 사용 (0xF2)

전송 데이터: [F2 42 01 40]
- 0xF2: 12채널 패킷 헤더
- 0x42: 채널 1-4 (LSB first)
- 0x01: 채널 5-8
- 0x40: 채널 9-12
```

---

## 10. 현재 프로젝트와의 차이점

| 항목 | examples/2 | 현재 프로젝트 |
|------|------------|---------------|
| 아키텍처 | 4계층 | 5계층 |
| 언어 | C++ | C/C++ 혼용 |
| 스위처 지원 | ATEM, OBS, vMix | 없음 |
| Tally 패킹 | 가변 길이 PackedData | 구현 예정 |
| 듀얼모드 | 지원 | 미지원 |
| LoRa 패킷 | 헤더 기반 가변 길이 | 단일 고정 길이 |

---

## 11. 참고 문서

- `examples/2/docs/switcher_communication_and_packing_strategy.md` - 스위처 통신 및 패킹 전략 상세
