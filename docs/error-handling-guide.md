# Error Handling Guide

## Overview

Tally Node 프로젝트의 일관된 에러 처리를 위한 가이드라인입니다.

## Current Codebase Analysis

### 현재 사용 현황

| 레이어 | 주요 반환 타입 | 파일 예시 |
|------|---------------|----------|
| **05_hal** | `esp_err_t` | `wifi_hal.c`, `ethernet_hal.c`, `lora_hal.cpp` |
| **04_driver** | `bool` | `atem_driver.cpp`, `vmix_driver.cpp`, `wifi_driver.cpp` |
| **03_service** | `esp_err_t` | `hardware_service.cpp`, `switcher_service.cpp` |
| **02_presentation** | `esp_err_t`, `bool` | `web_server.cpp`, `DisplayManager.cpp` |
| **00_common** | `esp_err_t` | `event_bus.c`, `t_log.h` |

**발견**: 82개 파일에서 1,036회 ESP 에러 매크로/타입 사용

### 기존 패턴 문제점

1. **혼합 반환 타입**: 동일 계층에서 `esp_err_t`와 `bool` 혼용
2. **NULL 체크 불일치**: 일부 함수만 NULL 체크 수행
3. **에러 무시**: `ESP_ERROR_CHECK` 없는 ESP-IDF API 호출

---

## Return Type Guidelines (Standardized)

### 1. esp_err_t (ESP-IDF 연동 계층)

**적용 대상**: HAL, Event Bus, Service Layer

```cpp
// 05_hal, 03_service, 00_common
esp_err_t lora_hal_init(void);
esp_err_t event_bus_init(void);
esp_err_t hardware_service_init(void);
```

### 2. bool (드라이버 계층)

**적용 대상**: 04_driver (하드웨어 추상화 위주)

```cpp
// 단순 초기화 성공/실패만 필요한 경우
bool wifi_driver_init(void);
bool atem_driver_initialize(void);
```

### 3. void (복구 불가능 오류)

**사용 조건**: 시스템 치명적 오류

```cpp
void critical_init() {
    if (xSemaphoreCreateMutex() == NULL) {
        T_LOGE(TAG, "Failed to create mutex");
        abort();
    }
}
```

---

## Error Handling Macros

### 기존 `t_log.h` 구현 유지

```cpp
// components/00_common/t_log/include/t_log.h (기존 코드 유지)
#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x) do { \
    esp_err_t err_rc = (x); \
    if (unlikely(err_rc != ESP_OK)) { \
        T_LOGE(ERR, "ESP_ERROR_CHECK failed: %s:%d - err=0x%x", __FILE__, __LINE__, err_rc); \
        abort(); \
    } \
} while(0)
#endif
```

### 추가 매크로 (새로 도입)

**파일**: `components/00_common/app_types/include/error_macros.h`

```cpp
#ifndef ERROR_MACROS_H
#define ERROR_MACROS_H

#include "esp_err.h"

// esp_err_t 반환 함수 체인용
#define RETURN_IF_ERROR(x) do { \
    esp_err_t _err = (x); \
    if (_err != ESP_OK) return _err; \
} while(0)

// 조건부 에러 반환
#define RETURN_ERROR_IF(cond, err) do { \
    if (cond) return (err); \
} while(0)

// bool 반환 함수용 NULL 체크
#define RETURN_BOOL_IF_NULL(ptr) do { \
    if (!(ptr)) { \
        T_LOGE(TAG, "NULL argument: %s", #ptr); \
        return false; \
    } \
} while(0)

// esp_err_t 반환 함수용 NULL 체크
#define RETURN_ERR_IF_NULL(ptr) do { \
    if (!(ptr)) { \
        T_LOGE(TAG, "NULL argument: %s", #ptr); \
        return ESP_ERR_INVALID_ARG; \
    } \
} while(0)

#endif // ERROR_MACROS_H
```

---

## Layer-Specific Patterns

### HAL Layer (05_hal)

```cpp
// 좋음 (현재 패턴 유지)
esp_err_t lora_hal_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    esp_err_t err = spi_bus_initialize(...);
    if (err != ESP_OK) {
        T_LOGE(TAG, "SPI bus init failed: 0x%x", err);
        return err;
    }
    
    s_initialized = true;
    return ESP_OK;
}
```

### Driver Layer (04_driver)

```cpp
// 좋음 (현재 패턴 유지)
bool AtemDriver::initialize() {
    if (sock_fd_ >= 0) {
        return true;  // 이미 초기화됨
    }
    
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd_ < 0) {
        T_LOGE(TAG, "fail:socket:%d", errno);
        return false;
    }
    
    // 설정 로직...
    return true;
}
```

### Service Layer (03_service)

```cpp
// 개선 필요: NULL 체크 추가
esp_err_t switcher_service_initialize(switcher_service_handle_t handle) {
    RETURN_ERR_IF_NULL(handle);
    
    auto* service = static_cast<SwitcherService*>(handle);
    return service->init() ? ESP_OK : ESP_FAIL;
}
```

### Event Handlers

```cpp
// 현재 패턴 (유지)
static esp_err_t onConfigDataEvent(const event_data_t* event) {
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_switcher_service_instance) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 처리 로직...
    return ESP_OK;
}
```

---

## Log Error Levels (현재 패턴)

```cpp
// 치명적 오류 - 시스템 중단 가능
T_LOGE(TAG, "Failed to allocate memory: 0x%x", err);

// 경고 - 기능 제한 but 동작 가능
T_LOGW(TAG, "Secondary switcher not connected, using primary only");

// 정보 - 정상적인 상태 변경
T_LOGI(TAG, "Switcher connected: %s@%s", name, ip);

// 디버그 - 상세 정보
T_LOGD(TAG, "Packet received: len=%d, rssi=%d", len, rssi);
```

---

## Implementation Plan

### Phase 1: 매크로 파일 추가 (1일)

- [ ] `components/00_common/app_types/include/error_macros.h` 생성
- [ ] `CMakeLists.txt`에 헤더 포함
- [ ] 기존 `ESP_ERROR_CHECK` 유지

### Phase 2: Service Layer 정규화 (3일)

대상 파일:
- `switcher_service.cpp`
- `hardware_service.cpp`
- `network_service.cpp`
- `lora_service.cpp`

변경:
- NULL 체크 매크로 적용
- `esp_err_t` 반환 통일

### Phase 3: Event Handlers 정규화 (2일)

대상 파일:
- `web_server_events.cpp`
- 각 Service 이벤트 핸들러

변경:
- `onNetworkStatusEvent` 패턴 적용
- NULL 체크 매크로 적용

---

## Migration Checklist

### 파일별 점검

- [ ] `components/05_hal/*/*.c` - `esp_err_t` 반환 확인
- [ ] `components/04_driver/*/*.cpp` - `bool` 반환 확인
- [ ] `components/03_service/*/*.cpp` - 일관된 에러 처리
- [ ] `components/02_presentation/*/*.cpp` - 에러 전파 확인
- [ ] `components/01_app/*.cpp` - 최상위 에러 처리

### 공통 점검 항목

- [ ] NULL 포인터 체크 모든 함수 진입부에 추가
- [ ] ESP-IDF API 호출 후 에러 확인
- [ ] 로그 레벨 적절한지 확인
- [ ] 에러 전파 (early return) 적용
- [ ] `abort()` 사용은 치명적 오류에만

---

## References

- ESP-IDF Error Codes: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/error-codes.html
- ESP-IDF 5.5 Migration Guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-5.x/5.5/
