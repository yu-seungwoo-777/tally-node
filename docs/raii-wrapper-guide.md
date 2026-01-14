# RAII Wrapper Guide

## Overview

`packed_data_t`의 수동 메모리 관리를 RAII 래퍼 클래스로 대체하여 메모리 안전성을 향상시키는 가이드입니다.

## Current Codebase Analysis

### packed_data_t 사용 현황

| 파일 | init/cleanup 호출 수 | 주요 사용처 |
|------|---------------------|-------------|
| `switcher_service.cpp` | 13회 (init: 3, cleanup: 10) | `combined_packed_`, `SwitcherInfo::last_packed` |
| `atem_driver.cpp` | 3회 (init: 1, cleanup: 2) | `cached_packed_` (소멸자, 재초기화) |
| `vmix_driver.cpp` | 3회 (init: 1, cleanup: 2) | `cached_packed_` (소멸자, 재초기화) |
| `prod_tx_app.cpp` | 3회 (cleanup only) | `s_app.last_tally` |
| `TallyTypes.cpp` | 4회 | 내부 구현 (copy, reset) |

**총 26회의 manual init/cleanup 호출** - 조기 리턴 시 메모리 누수 위험

### 현재 패턴 분석

#### Driver Layer Pattern (cached_packed_)

```cpp
// components/04_driver/switcher_driver/atem/atem_driver.cpp
AtemDriver::AtemDriver(const AtemConfig& config)
    : config_(config), state_(), conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1), tally_callback_(), connection_callback_()
{
    // 초기화: nullptr로 설정
    cached_packed_.data = nullptr;
    cached_packed_.data_size = 0;
    cached_packed_.channel_count = 0;
}

AtemDriver::~AtemDriver() {
    disconnect();
    packed_data_cleanup(&cached_packed_);  // 수동 정리
}

void AtemDriver::onTallyData(...) {
    // 채널 수 변경 시 재초기화
    if (channel_count != cached_packed_.channel_count) {
        packed_data_cleanup(&cached_packed_);
        packed_data_init(&cached_packed_, channel_count);
    }
}
```

**문제점**: 
- 생성자에서 초기화하지 않음 (nullptr만 설정)
- 첫 사용 시 `packed_data_init` 호출 필요
- 재초기화 로직에서 cleanup/init 쌍 관리 필요

#### Service Layer Pattern (combined_packed_)

```cpp
// components/03_service/switcher_service/switcher_service.cpp
SwitcherService::SwitcherService()
    : primary_(), secondary_(), dual_mode_enabled_(false)
{
    // nullptr로만 설정
    combined_packed_.data = nullptr;
    combined_packed_.data_size = 0;
    combined_packed_.channel_count = 0;
}

SwitcherService::~SwitcherService() {
    stop();
    primary_.cleanup();
    secondary_.cleanup();
    packed_data_cleanup(&combined_packed_);  // 수동 정리
}

packed_data_t SwitcherService::combineDualModeTally() const {
    // 채널 수 변경 시 재초기화
    if (combined_packed_.channel_count != max_channel_used) {
        packed_data_cleanup(&combined_packed_);
        packed_data_init(&combined_packed_, max_channel_used);
    }
    // ...
}
```

**문제점**:
- 생성자에서 초기화하지 않음
- 첫 결합 시 init 필요
- 복잡한 재초기화 로직

---

## Problem

### 현재 코드의 문제점

```cpp
void processTally() {
    packed_data_t tally;
    packed_data_init(&tally, 20);
    
    if (some_condition) {
        return;  // 메모리 누수! cleanup 누락
    }
    
    packed_data_cleanup(&tally);
}
```

**문제**:
- `packed_data_cleanup()` 호출 깜빡하면 메모리 누수
- 예외/조기 리턴 시 누수 위험
- 포인터 전달로 인한 소유권 모호

## Solution: PackedData RAII Class

### 헤더 파일 생성

**파일**: `components/00_common/tally_types/include/PackedData.h`

```cpp
#ifndef PACKED_DATA_H
#define PACKED_DATA_H

#include "TallyTypes.h"
#include <stdint.h>
#include <utility>

/**
 * @brief RAII 래퍼 - PackedData 자동 메모리 관리
 *
 * 스코프를 벗어나면 자동으로 메모리 정리
 * 이동 의미론(move semantics) 지원
 */
class PackedData {
public:
    // ========================================================================
    // 생성자 / 소멸자
    // ========================================================================
    
    /**
     * @brief 생성자
     * @param channel_count 채널 수 (기본값: TALLY_MAX_CHANNELS)
     */
    explicit PackedData(uint8_t channel_count = TALLY_MAX_CHANNELS) {
        packed_data_init(&data_, channel_count);
    }

    /**
     * @brief 소멸자 - 자동 메모리 정리
     */
    ~PackedData() {
        packed_data_cleanup(&data_);
    }

    // ========================================================================
    // 복사 / 이동
    // ========================================================================
    
    // 복사 금지 (명확한 의도 전달)
    PackedData(const PackedData&) = delete;
    PackedData& operator=(const PackedData&) = delete;

    /**
     * @brief 이동 생성자
     */
    PackedData(PackedData&& other) noexcept {
        data_ = other.data_;
        other.data_ = {nullptr, 0, 0};  // 소유권 이전
    }

    /**
     * @brief 이동 대입 연산자
     */
    PackedData& operator=(PackedData&& other) noexcept {
        if (this != &other) {
            packed_data_cleanup(&data_);  // 기존 데이터 정리
            data_ = other.data_;
            other.data_ = {nullptr, 0, 0};
        }
        return *this;
    }

    // ========================================================================
    // 접근자 (Accessors)
    // ========================================================================
    
    /**
     * @brief 내부 데이터 포인터 반환 (const)
     */
    const packed_data_t* get() const { return &data_; }
    
    /**
     * @brief 내부 데이터 포인터 반환
     */
    packed_data_t* get() { return &data_; }

    /**
     * @brief 화살표 연산자 오버로딩
     */
    const packed_data_t* operator->() const { return &data_; }
    packed_data_t* operator->() { return &data_; }

    /**
     * @brief 역참조 연산자 오버로딩
     */
    const packed_data_t& operator*() const { return data_; }
    packed_data_t& operator*() { return data_; }

    // ========================================================================
    // 유틸리티 메서드
    // ========================================================================
    
    /**
     * @brief 채널 상태 설정
     * @param channel 채널 번호 (1-based)
     * @param flags 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
     */
    void setChannel(uint8_t channel, uint8_t flags) {
        packed_data_set_channel(&data_, channel, flags);
    }

    /**
     * @brief 채널 상태 조회
     * @param channel 채널 번호 (1-based)
     * @return 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
     */
    uint8_t getChannel(uint8_t channel) const {
        return packed_data_get_channel(&data_, channel);
    }

    /**
     * @brief 다른 PackedData와 비교
     * @param other 비교 대상
     * @return 동일하면 true
     */
    bool equals(const PackedData& other) const {
        return packed_data_equals(&data_, &other.data_);
    }

    /**
     * @brief 유효성 확인
     * @return 유효하면 true
     */
    bool isValid() const {
        return packed_data_is_valid(&data_);
    }

    /**
     * @brief 64비트 정수로 변환
     * @return 64비트 packed 값
     */
    uint64_t toUint64() const {
        return packed_data_to_uint64(&data_);
    }

    /**
     * @brief 16진수 문자열로 변환
     * @param buf 출력 버퍼
     * @param buf_size 버퍼 크기
     * @return 버퍼 포인터
     */
    char* toHex(char* buf, size_t buf_size) const {
        return packed_data_to_hex(&data_, buf, buf_size);
    }

    /**
     * @brief Tally 문자열로 포맷
     * @param buf 출력 버퍼
     * @param buf_size 버퍼 크기
     * @return 버퍼 포인터
     */
    char* formatTally(char* buf, size_t buf_size) const {
        return packed_data_format_tally(&data_, buf, buf_size);
    }

    /**
     * @brief 채널 수 조회
     */
    uint8_t channelCount() const { return data_.channel_count; }

    /**
     * @brief 채널 수 변경 (재초기화)
     * @param new_count 새 채널 수
     */
    void resize(uint8_t new_count) {
        if (data_.channel_count != new_count) {
            packed_data_cleanup(&data_);
            packed_data_init(&data_, new_count);
        }
    }

private:
    packed_data_t data_;
};

// ============================================================================
// 비교 연산자
// ============================================================================

inline bool operator==(const PackedData& a, const PackedData& b) {
    return a.equals(b);
}

inline bool operator!=(const PackedData& a, const PackedData& b) {
    return !a.equals(b);
}

#endif // PACKED_DATA_H
```

## Usage Examples

### 기본 사용

```cpp
#include "PackedData.h"

void processTally() {
    PackedData tally(20);  // 자동 초기화
    
    tally.setChannel(1, TALLY_STATUS_PROGRAM);
    tally.setChannel(2, TALLY_STATUS_PREVIEW);
    
    if (tally.getChannel(1) == TALLY_STATUS_PROGRAM) {
        // Program 처리
    }
    
    // 스코프 종료 시 자동 정리
}
```

### Driver Layer 마이그레이션 (AtemDriver)

**변경 전**:
```cpp
// atem_driver.cpp
AtemDriver::AtemDriver(const AtemConfig& config)
    : config_(config), state_(), conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1), tally_callback_()
{
    cached_packed_.data = nullptr;
    cached_packed_.data_size = 0;
    cached_packed_.channel_count = 0;
}

AtemDriver::~AtemDriver() {
    disconnect();
    packed_data_cleanup(&cached_packed_);
}

void AtemDriver::onTallyData(...) {
    if (channel_count != cached_packed_.channel_count) {
        packed_data_cleanup(&cached_packed_);
        packed_data_init(&cached_packed_, channel_count);
    }
}
```

**변경 후**:
```cpp
// atem_driver.h
class AtemDriver {
private:
    PackedData cached_packed_;  // RAII 래퍼 사용
};

// atem_driver.cpp
AtemDriver::AtemDriver(const AtemConfig& config)
    : config_(config), state_(), conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1), tally_callback_()
    , cached_packed_(20)  // 생성자에서 바로 초기화
{
    // 별도 초기화 불필요
}

AtemDriver::~AtemDriver() {
    disconnect();
    // 자동 정리 - 별도 cleanup 불필요
}

void AtemDriver::onTallyData(...) {
    // resize() 메서드로 간단히 재초기화
    cached_packed_.resize(channel_count);
}
```

### Service Layer 마이그레이션 (SwitcherService)

**변경 전**:
```cpp
// switcher_service.cpp
SwitcherService::SwitcherService()
    : primary_(), secondary_(), dual_mode_enabled_(false)
{
    combined_packed_.data = nullptr;
    combined_packed_.data_size = 0;
    combined_packed_.channel_count = 0;
}

SwitcherService::~SwitcherService() {
    stop();
    primary_.cleanup();
    secondary_.cleanup();
    packed_data_cleanup(&combined_packed_);
}

packed_data_t SwitcherService::combineDualModeTally() const {
    if (combined_packed_.channel_count != max_channel_used) {
        packed_data_cleanup(&combined_packed_);
        packed_data_init(&combined_packed_, max_channel_used);
    }
    // ...
}
```

**변경 후**:
```cpp
// switcher_service.h
class SwitcherService {
private:
    mutable PackedData combined_packed_;  // mutable로 const 메서드에서도 resize 가능
};

// switcher_service.cpp
SwitcherService::SwitcherService()
    : primary_(), secondary_()
    , dual_mode_enabled_(false)
    , combined_packed_(TALLY_MAX_CHANNELS)  // 생성자에서 초기화
{
    // 별도 초기화 불필요
}

SwitcherService::~SwitcherService() {
    stop();
    primary_.cleanup();
    secondary_.cleanup();
    // 자동 정리
}

packed_data_t SwitcherService::combineDualModeTally() const {
    combined_packed_.resize(max_channel_used);
    // ...
}
```

### SwitcherInfo 구조체 마이그레이션

**변경 전**:
```cpp
// switcher_service.h
struct SwitcherInfo {
    std::unique_ptr<ISwitcherPort> adapter;
    packed_data_t last_packed;
    bool has_changed;
    
    SwitcherInfo() : adapter(nullptr), last_packed{nullptr, 0, 0}, has_changed(false) {}
    
    void cleanup() {
        adapter.reset();
        if (last_packed.data) {
            packed_data_cleanup(&last_packed);
            last_packed.data = nullptr;
            last_packed.data_size = 0;
            last_packed.channel_count = 0;
        }
        has_changed = false;
    }
};
```

**변경 후**:
```cpp
// switcher_service.h
struct SwitcherInfo {
    std::unique_ptr<ISwitcherPort> adapter;
    PackedData last_packed;  // RAII 래퍼
    bool has_changed;
    
    SwitcherInfo() 
        : adapter(nullptr)
        , last_packed(TALLY_MAX_CHANNELS)  // 자동 초기화
        , has_changed(false) 
    {}
    
    void cleanup() {
        adapter.reset();
        has_changed = false;
        // last_packed은 자동 정리
    }
};
```

### 함수 인자 전달

```cpp
// 참조 전달 (const)
void printTally(const PackedData& tally) {
    char buf[64];
    T_LOGI(TAG, "Tally: %s", tally.formatTally(buf, sizeof(buf)));
}

// 소유권 이동 전달
void storeTally(PackedData&& tally) {
    // 소유권 이전
    storedTally_ = std::move(tally);
}
```

## Migration Strategy

### 단계 1: RAII 클래스 추가

1. `PackedData.h` 파일 생성
2. 기존 코드와 호환되는 인터페이스 제공
3. `CMakeLists.txt`에 헤더 포함 경로 추가

### 단계 2: 새 코드에서 사용

```cpp
// 새로 작성하는 코드에서 PackedData 사용
void newFunction() {
    PackedData tally;  // RAII 사용
    // ...
}
```

### 단계 3: 점진적 마이그레이션 우선순위

1. **High Priority** (메모리 누수 위험 높음):
   - `switcher_service.cpp` - 13회 호출
   - `prod_tx_app.cpp` - 3회 cleanup

2. **Medium Priority** (드라이버 계층):
   - `atem_driver.cpp` - 3회 호출
   - `vmix_driver.cpp` - 3회 호출

3. **Low Priority** (내부 구현):
   - `TallyTypes.cpp` - 4회 호출

### 단계 4: 호환성 유지

```cpp
// 기존 C 인터페이스 유지
packed_data_t switcher_service_get_combined_tally(switcher_service_handle_t handle) {
    auto* service = static_cast<SwitcherService*>(handle);
    // PackedData에서 내부 데이터 반환
    return *service->getCombinedTallyRef().get();
}
```

## Implementation Plan

### Phase 1: RAII 클래스 생성 (1일)

- [ ] `components/00_common/tally_types/include/PackedData.h` 생성
- [ ] `CMakeLists.txt`에 헤더 포함 경로 추가
- [ ] 단위 테스트 작성

### Phase 2: Service Layer 마이그레이션 (2일)

- [ ] `SwitcherService::combined_packed_` → `PackedData`
- [ ] `SwitcherInfo::last_packed` → `PackedData`
- [ ] 관련 cleanup 코드 제거
- [ ] 테스트 및 검증

### Phase 3: Driver Layer 마이그레이션 (1일)

- [ ] `AtemDriver::cached_packed_` → `PackedData`
- [ ] `VmixDriver::cached_packed_` → `PackedData`
- [ ] 관련 cleanup 코드 제거
- [ ] 테스트 및 검증

### Phase 4: App Layer 마이그레이션 (1일)

- [ ] `prod_tx_app.cpp` 마이그레이션
- [ ] 기타 앱 계층 코드 확인
- [ ] 전체 테스트

## Benefits

1. **메모리 안전성**: 스코프 종료 시 자동 정리
2. **예외 안전**: 조기 리턴/예외 시에도 정리 보장
3. **명확한 소유권**: 이동 의미론으로 소유권 명확
4. **코드 간소화**: cleanup 코드 제거
5. **resize() 메서드**: 재초기화 로직 간소화

## Trade-offs

| 장점 | 단점 |
|-----|-----|
| 자동 메모리 관리 | 약간의 오버헤드 (무시할 수준) |
| 명확한 소유권 | C 인터페이스와 호환성 고려 필요 |
| 타입 안전성 | 추가 파일 생성 |
| 코드 간소화 | 기존 코드 수정 필요 |

## Migration Checklist

### 파일별 점검

- [ ] `components/03_service/switcher_service/switcher_service.cpp`
  - [ ] `combined_packed_` → `PackedData`
  - [ ] `SwitcherInfo::last_packed` → `PackedData`
  - [ ] cleanup 호출 제거
  - [ ] resize() 메서드 사용

- [ ] `components/04_driver/switcher_driver/atem/atem_driver.cpp`
  - [ ] `cached_packed_` → `PackedData`
  - [ ] 생성자 초기화 수정
  - [ ] 소멸자 cleanup 제거
  - [ ] resize() 메서드 사용

- [ ] `components/04_driver/switcher_driver/vmix/vmix_driver.cpp`
  - [ ] `cached_packed_` → `PackedData`
  - [ ] 생성자 초기화 수정
  - [ ] 소멸자 cleanup 제거
  - [ ] resize() 메서드 사용

- [ ] `components/01_app/prod_tx_app/prod_tx_app.cpp`
  - [ ] `s_app.last_tally` → `PackedData`

### 공통 점검 항목

- [ ] NULL 포인터 체크 제거 (RAII에서 불필요)
- [ ] 수동 init/cleanup 호출 제거
- [ ] 생성자 초기화 목록 확인
- [ ] 소멸자 cleanup 코드 제거
- [ ] C 인터페이스 호환성 확인
