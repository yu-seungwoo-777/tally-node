# 스위처 통신 및 패킹 전략

## 개요

본 문서는 듀얼 모드 Tally 시스템에서 스위처 통신, 데이터 패킹, 및 맵핑 전략을 설명한다.

## 아키텍처 원칙

### 레이어 분리
1. **스위처 코어 레이어**: 단일 스위처와의 1:1 통신, 원본 데이터 처리
2. **맵핑 레이어**: 다중 스위처 데이터 결합, 오프셋 적용, 마스킹
3. **전송 레이어**: 최종 Tally 데이터를 LoRa 패킷으로 변환

## 1. 스위처 코어 레이어

### 역할
- 각 스위처와 독립적으로 통신
- 원본 Tally 데이터를 packed 형태로 수신
- 오프셋 개념 없이 스위처 자체의 채널 번호만 관리

### 데이터 형식
각 스위처는 64비트 packed Tally 데이터 생성:
```
비트 구조: [채널N][채널N-1]...[채널2][채널1]
각 채널: 2비트 (00=None, 01=PGM, 10=PVW, 11=Both)
```

### 예시
```
Primary ATEM (4채널):
- PGM: 1, PVW: 4
- Packed: 0x0000000000000042
  - 비트 0-1: 01 (채널 1: PGM)
  - 비트 6-7: 10 (채널 4: PVW)

Secondary ATEM (6채널):
- PGM: 5, PVW: 6
- Packed: 0x0000000000001400
  - 비트 8-9: 01 (채널 5: PGM)
  - 비트 10-11: 10 (채널 6: PVW)
```

## 2. 맵핑 레이어 (FastTallyMapper)

### 역할
- 여러 스위처의 원본 Tally 데이터를 단일 Tally로 결합
- 각 스위처에 오프셋 적용
- 채널 수 제한을 통한 마스킹

### 설정 정보
```cpp
struct SwitcherConfig {
    uint8_t offset;        // 시작 채널 번호 (0-based)
    uint8_t channel_limit; // 최대 채널 수
    bool enabled;
};

// 듀얼 모드 예시
configs[0] = {0, 8, true};   // Primary: 채널 1-8
configs[1] = {4, 6, true};   // Secondary: 채널 5-10
```

### 맵핑 프로세스

#### 단계 1: 오프셋 적용 (Shift)
```cpp
// Secondary Tally: 0x1400 (채널 5,6)
// Offset: 4
uint64_t shifted = 0x1400 << (4 * 2) = 0x14000000
// 결과: 채널 9,10으로 이동
```

#### 단계 2: 마스킹 (Channel Limit)
```cpp
// Secondary 마스크: 6채널 * 2비트 = 12비트
uint64_t mask = ((1ULL << 12) - 1) << (4 * 2) = 0xFFF0000
uint64_t masked = shifted & mask = 0x14000000
```

#### 단계 3: 결합 (OR)
```cpp
uint64_t primary = 0x42;          // 채널 1,4
uint64_t secondary = 0x14000000;  // 채널 9,10
uint64_t combined = primary | secondary = 0x14000042
```

### 최종 결과
```
Combined Tally: 0x14000042
- 채널 1: PGM (Primary)
- 채널 4: PVW (Primary)
- 채널 9: PGM (Secondary 5 + offset 4)
- 채널 10: PVW (Secondary 6 + offset 4)
```

## 3. 마스킹 (Channel Limit)

```cpp
// Primary: 8채널만 허용 (비트 0-15)
uint64_t primary_limit = 8;
uint64_t primary_mask = (1ULL << (primary_limit * 2)) - 1;
uint64_t primary_masked = tally_primary & primary_mask;
// 결과: 0x0000000000000042 (그대로)

// Secondary: 6채널만 허용 (offset 4부터 6채널)
uint64_t secondary_offset = 4;
uint64_t secondary_limit = 6;
uint64_t secondary_mask = ((1ULL << (secondary_limit * 2)) - 1)
                         << (secondary_offset * 2);
// secondary_mask = 0xFFF0000
uint64_t secondary_masked = secondary_shifted & secondary_mask;

// 중복 영역 처리 (Secondary가 Primary 영역 침범 시)
if (secondary_offset < primary_limit) {
    // Primary에서 중복 영역 마스킹
    uint64_t overlap_start = secondary_offset;
    uint64_t overlap_bits = primary_limit - overlap_start;
    uint64_t overlap_mask = ((1ULL << (overlap_bits * 2)) - 1)
                           << (overlap_start * 2);
    primary_masked &= ~overlap_mask;
}
```

## 4. 전송 레이어

### 패킷 구조 (개선)

#### 4종류 패킷 헤더
```
8채널 패킷:   [0xF1][Data(2B)]  - 최대 8채널 (16비트)
12채널 패킷:  [0xF2][Data(3B)]  - 최대 12채널 (24비트)
16채널 패킷:  [0xF3][Data(4B)]  - 최대 16채널 (32비트)
20채널 패킷:  [0xF4][Data(5B)]  - 최대 20채널 (40비트)
```

#### 헤더 정의
- **0xF1**: 8채널 Tally 패킷 (총 3바이트)
- **0xF2**: 12채널 Tally 패킷 (총 4바이트)
- **0xF3**: 16채널 Tally 패킷 (총 5바이트)
- **0xF4**: 20채널 Tally 패킷 (총 6바이트)

#### 장점
1. **고정 길이 패킷**: 헤더 하나로 데이터 길이 즉시 알 수 있음
2. **효율적 전송**: 필요한 만큼만 전송 (8/12/16/20채널 선택)
3. **단순한 파싱**: 헤더 값으로 데이터 길이 계산
4. **확장성**: 향후 0xF5-0xFF까지 예약 가능

### 데이터 압축
```cpp
// Combined Tally: 0x14000042
// 최대 채널: 10 → 12채널 패킷 사용 (0xF2)
// 압축 순서: LSB → MSB
// 채널 1-8: 0x42 01
// 채널 9-12: 0x40 00

// 전송 데이터: [F2 42 01 40]
// - 0xF2: 12채널 패킷 헤더
// - 0x42: 채널 1-4 (LSB first)
// - 0x01: 채널 5-8
// - 0x40: 채널 9-12
```

## 5. 수신 측 처리

### 패킷 파싱
```cpp
// 수신 데이터: [F2 42 01 40]
uint8_t header = data[0];
uint64_t tally = 0;
uint8_t max_channels = 0;
uint8_t data_length = 0;

// 헤더에 따른 채널 수와 데이터 길이 결정
switch (header) {
    case 0xF1: max_channels = 8;  data_length = 2; break;
    case 0xF2: max_channels = 12; data_length = 3; break;
    case 0xF3: max_channels = 16; data_length = 4; break;
    case 0xF4: max_channels = 20; data_length = 5; break;
    default: return;  // 잘못된 헤더
}

// 바이트를 64비트로 재조립
for (int i = 0; i < data_length; i++) {
    tally |= ((uint64_t)data[1 + i]) << (i * 8);
}

// 결과: 0x14000042
```

### 채널별 Tally 추출
```cpp
for (int channel = 1; channel <= max_channels; channel++) {
    uint8_t shift = (channel - 1) * 2;
    uint8_t tally_value = (tally >> shift) & 0x03;

    if (tally_value == 0) continue;  // 비활성 채널은 건너뛰기

    bool is_pgm = (tally_value == 1 || tally_value == 3);
    bool is_pvw = (tally_value == 2 || tally_value == 3);

    // 채널별 LED 제어
    set_tally_light(channel, is_pgm, is_pvw);
}
```

## 5. 확장성

### 3중 스위처 지원
```cpp
configs[0] = {0, 8, true};   // Primary
configs[1] = {8, 6, true};   // Secondary
configs[2] = {14, 4, true};  // Tertiary

// 맵핑 예시
// Primary: 채널 1-8
// Secondary: 채널 9-14
// Tertiary: 채널 15-18
```

### 동적 설정 변경
- NVS에서 설정 읽기
- 웹 인터페이스를 통한 실시간 변경
- 설정 변경 시 FastTallyMapper 재초기화

## 6. 성능 최적화

### 미리 계산된 값
- 마스크 값을 설정 시 계산하여 캐시
- 시프트량을 테이블로 관리

### 비트 연산 최적화
```cpp
// 나쁜 예: 반복적인 계산
for (int i = 0; i < count; i++) {
    uint64_t mask = ((1ULL << (limit * 2)) - 1) << (offset * 2);
    result |= (data[i] << (offset * 2)) & mask;
}

// 좋은 예: 미리 계산
uint64_t shift = cached_shift[i];
uint64_t mask = cached_mask[i];
result |= (data[i] << shift) & mask;
```

## 7. 오류 처리

### 데이터 무결성 검사
- 패킷 헤더 확인
- 채널 수 범위 검증 (1-20)
- 체크섬 추가 (필요 시)

### 동기화
- 스위처 연결 끊김 시 해당 Tally를 0으로 처리
- 재연결 시 자동 초기화
- 타임아웃 핸들링

## 구현 순서

1. **SwitcherManager**: 원본 Tally 조회 기능 구현
2. **FastTallyMapper**: 오프셋, 마스킹, 결합 로직 및 패킷 타입 결정 기능 구현
3. **TallyDispatcher**: 패킷 생성 및 전송 로직 구현
4. **RX 측**: 패킷 수신 및 파싱, LED 제어 구현
5. **통합 테스트**: 듀얼 모드 시나리오 검증

## 채널 수 결정 방법

FastTallyMapper는 다음을 통해 최대 채널 수를 계산합니다:

1. **설정 기반 계산**:
   ```cpp
   max_channel = max(switcher[i].offset + switcher[i].channel_limit)
   ```

2. **예시**:
   - Primary: offset=0, limit=8 → 채널 1-8
   - Secondary: offset=4, limit=6 → 채널 5-10
   - 최대 채널 = max(8, 10) = 10

3. **패킷 타입 결정**:
   - 최대 8채널 → 0xF1
   - 최대 12채널 → 0xF2
   - 최대 16채널 → 0xF3
   - 최대 20채널 → 0xF4

4. **실제 데이터 기반 최적화** (선택 사항):
   - 활성 채널만 스캔하여 실제 필요한 패킷 크기 결정
   - 예: 20채널 설정이지만 채널 1-10만 사용 → 12채널 패킷 사용