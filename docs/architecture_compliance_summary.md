# 아키텍처 준수 요약

## 현재 상태

### 문서와 실제 구현의 계층 비교

#### 4계층 아키텍처 (architecture.md)
```
┌─────────────────────────────────────┐
│ Application Layer (main.cpp)       │
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│ Domain Manager Layer               │
│ ├─ DisplayManager                  │
│ ├─ CommunicationManager            │
│ │  └─ TallyDispatcher             │
│ │  └─ FastTallyMapper              │ ← Domain Layer에서 동작
│ └─ NetworkManager                 │
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│ Core Service Layer                 │
│ ├─ SwitcherManager                 │
│ └─ InfoManager                     │
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│ Infrastructure Layer               │
│ ├─ LoRaManager                     │
│ └─ ButtonPoll                      │
└─────────────────────────────────────┘
```

#### 스위처 통신 문서 (switcher_communication_and_packing_strategy.md)
```
1. 스위처 코어 레이어     → Core Service Layer (SwitcherManager)
2. 맵핑 레이어          → Domain Manager Layer (FastTallyMapper)
3. 전송 레이어          → Domain Manager Layer (TallyDispatcher)
```

### 준수 상태

✅ **올바르게 배치됨**
- SwitcherManager: Core Service Layer
- TallyDispatcher: Domain Manager Layer (CommunicationManager 내)
- FastTallyMapper: Domain Manager Layer (TallyDispatcher에서 사용)
- LoRaManager: Infrastructure Layer

✅ **데이터 흐름**
1. SwitcherManager (Core) → 원본 Tally
2. FastTallyMapper (Domain) → 오프셋 적용 및 결합
3. TallyDispatcher (Domain) → 패킷 생성
4. LoRaManager (Infrastructure) → RF 전송

### 문제점 및 해결

#### 현재 문제
- TallyDispatcher.cpp에서 이중 오프셋 적용 시도
  - 139행: `tally_secondary >> (secondary_offset * 2)` (잘못된 원본 복원)
  - 151행: 다시 `<< (secondary_offset * 2)` (중복 적용)

#### 해결 방안
```cpp
// 현재 (잘못됨)
uint64_t secondary_original = tally_secondary >> (secondary_offset * 2);
uint64_t switcher_tally[4] = { tally_primary, secondary_original, 0, 0 };

// 수정 (올바름)
uint64_t switcher_tally[4] = { tally_primary, tally_secondary, 0, 0 };
```

### 권장 구현 순서

1. **SwitcherManager 수정**: `getTallyPacked()`에서 오프셋 로직 제거
2. **FastTallyMapper 구현**: 오프셋, 마스킹, 결합 로직 완성
3. **TallyDispatcher 수정**: 중복 오프셋 제거
4. **패킷 구조 개선**: F1-F4 헤더 도입

### 결론

문서는 4계층 아키텍처를 올바르게 반영하고 있으며, 실제 구현도 대부분 올바르게 배치되어 있습니다.
유일한 문제는 TallyDispatcher에서 불필요한 오프셋 조작을 시도하는 것으로, 이를 제거하면 아키텍처가 완벽하게 준수됩니다.