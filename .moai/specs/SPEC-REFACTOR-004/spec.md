# SPEC-REFACTOR-004: 로그 시스템 표준화 리팩토링

## TAG BLOCK

```
TAG: SPEC-REFACTOR-004
DOMAIN: REFACTOR
TITLE: Log System Standardization Refactoring
STATUS: Planned
PRIORITY: Medium
ASSIGNED: TBD
CREATED: 2026-01-15
UPDATED: 2026-01-15
```

## 1. 환경 (Environment)

### 1.1 프로젝트 컨텍스트

- **프로젝트**: Tally Node (ESP32-S3 기반 무선 탤리 시스템)
- **아키텍처**: Clean Architecture (6계층)
- **빌드 시스템**: PlatformIO with ESP-IDF 5.5.0

### 1.2 현재 상황

Tally Node 프로젝트는 ESP-IDF 기본 로그 시스템(`esp_log.h`)을 대체하여 커스텀 `t_log` 시스템을 사용합니다. 그러나 일부 코드에서 ESP_LOG 잔존이 발견되었습니다.

**검토 결과:**
- ESP_LOG 잔존: 2개소 (`u8g2_esp32_hal.c`)
- esp_log.h include: 11개 파일 (그중 1개에서 사용, 10개는 미사용)
- t_log 대체 완료: 대부분의 코드

### 1.3 영향 범위

| 계층 | 컴포넌트 | 파일 수 | 작업 유형 |
|------|---------|---------|-----------|
| 05_hal | display_hal/u8g2_hal | 1 | 수정 + include 제거 |
| 02_presentation | web_server | 6 | include 제거 |
| 05_hal | battery_hal, ws2812_hal, display_hal | 3 | include 제거 |
| **합계** | **10 컴포넌트** | **11 파일** | **리팩토링** |

## 2. 가정 (Assumptions)

### 2.1 기술적 가정

- **가정**: t_log.h의 ESP_ERROR_CHECK 매크로가 프로젝트 전체에서 사용 가능해야 한다
- **신뢰도**: High (t_log.h는 00_common 계층에 위치)
- **검증 방법**: 빌드 성공 확인

### 2.2 기능적 가정

- **가정**: hex dump 기능은 I2C 디버깅용으로 선택적 사용이 가능하다
- **신뢰도**: Medium (개발 중 디버깅에만 사용)
- **검증 방법**: 조건부 컴파일로 검증

### 2.3 호환성 가정

- **가정**: esp_log.h include 제거가 다른 컴포넌트에 영향을 주지 않는다
- **신뢰도**: High (Grep 검증으로 미사용 확인 완료)
- **검증 방법**: 전체 빌드 테스트

## 3. 요구사항 (Requirements)

### 3.1 Ubiquitous Requirements (항상 활성)

**REQ-001**: 시스템은 t_log.h를 표준 로깅 시스템으로 사용해야 한다.

**REQ-002**: 모든 로그 메시지는 영문으로 작성해야 한다.

**REQ-003**: 모든 주석은 한국어로 작성해야 한다.

### 3.2 Event-Driven Requirements (이벤트-응답)

**REQ-004**: **WHEN** esp_log.h include가 미사용 상태로 발견되면 **THEN** 해당 include를 제거해야 한다.

**REQ-005**: **WHEN** ESP_ERROR_CHECK 매크로가 필요하면 **THEN** t_log.h에서 제공하는 표준 매크로를 사용해야 한다.

### 3.3 State-Driven Requirements (상태-조건)

**REQ-006**: **IF** ESP_LOGE 매크로가 사용되면 **THEN** T_LOGE 매크로로 대체해야 한다.

**REQ-007**: **IF** ESP_LOG_BUFFER_HEXDUMP 매크로가 사용되면 **THEN** T_LOGI_HEX 매크로로 대체해야 한다.

### 3.4 Unwanted Requirements (금지 사항)

**REQ-008**: 시스템은 esp_log.h를 직접 include하지 **않아야 한다**.

**REQ-009**: ESP_ERROR_CHECK 매크로를 재정의하지 **않아야 한다**.

### 3.5 Optional Requirements (선택 사항)

**REQ-010**: **가능하면** t_log 시스템에 hex dump 매크로(T_LOGI_HEX)를 제공할 수 있다.

## 4. 명세 (Specifications)

### 4.1 ESP_ERROR_CHECK 표준화

**위치**: `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c`

**제거할 코드 (라인 22-30):**
```c
#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)                   \
  do {                                       \
    esp_err_t rc = (x);                      \
    if (rc != ESP_OK) {                      \
      ESP_LOGE("err", "esp_err_t = %d", rc); \
      assert(0 && #x);                       \
    }                                        \
  } while (0)
```

**대체 방법:**
- `#include "t_log.h"` 추가
- t_log.h의 ESP_ERROR_CHECK 매크로 사용 (라인 86-94 참조)

### 4.2 Hex Dump 매크로 추가

**위치**: `components/00_common/t_log/include/t_log.h`

**추가할 매크로:**
```c
// Hex dump 기본 매크로
#define T_LOG_BUFFER_HEX(level, tag, data, len) do { \
    if (level <= T_LOG_DEFAULT_LEVEL) { \
        const uint8_t* _data = (const uint8_t*)(data); \
        for (int i = 0; i < (len); i += 16) { \
            char _hex[64]; \
            int _pos = 0; \
            for (int j = 0; j < 16 && (i + j) < (len); j++) { \
                _pos += snprintf(_hex + _pos, sizeof(_hex) - _pos, "%02X ", _data[i + j]); \
            } \
            t_log_output(level, tag, "%04X: %s", i, _hex); \
        } \
    } \
} while(0)

// 편의 매크로
#define T_LOGE_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_ERROR, tag, data, len)
#define T_LOGW_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_WARN, tag, data, len)
#define T_LOGI_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_INFO, tag, data, len)
#define T_LOGD_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_DEBUG, tag, data, len)
#define T_LOGV_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_VERBOSE, tag, data, len)
```

**사용 예시:**
```c
// 기존 코드
ESP_LOG_BUFFER_HEXDUMP(TAG, data_ptr, arg_int, ESP_LOG_VERBOSE);

// 변경 후
T_LOGI_HEX(TAG, data_ptr, arg_int);
```

### 4.3 미사용 esp_log.h Include 제거

**대상 파일 (10개):**

| # | 파일 경로 |
|---|-----------|
| 1 | `components/02_presentation/web_server/web_server_config.cpp` |
| 2 | `components/02_presentation/web_server/web_server_events.cpp` |
| 3 | `components/02_presentation/web_server/web_server_json.cpp` |
| 4 | `components/02_presentation/web_server/handlers/api_status.cpp` |
| 5 | `components/02_presentation/web_server/handlers/api_license.cpp` |
| 6 | `components/02_presentation/web_server/handlers/api_devices.cpp` |
| 7 | `components/02_presentation/web_server/handlers/api_lora.cpp` |
| 8 | `components/05_hal/battery_hal/battery_hal.c` |
| 9 | `components/05_hal/ws2812_hal/ws2812_hal.c` |
| 10 | `components/05_hal/display_hal/display_hal.c` |

**작업:** 각 파일에서 `#include "esp_log.h"` 라인 제거

## 5. 추적성 (Traceability)

### 5.1 관련 문서

- `docs/LOG_SYSTEM_REVIEW.md` - 로그 시스템 검토 문서
- `components/00_common/t_log/include/t_log.h` - t_log 인터페이스
- `include/LogConfig.h` - 로그 설정

### 5.2 관련 컴포넌트

| 컴포넌트 | 영향 | 설명 |
|---------|------|------|
| t_log | 수정 | hex dump 매크로 추가 |
| u8g2_hal | 수정 | ESP_ERROR_CHECK 제거, esp_log.h 제거 |
| web_server | 수정 | esp_log.h include 제거 (6개 파일) |
| battery_hal | 수정 | esp_log.h include 제거 |
| ws2812_hal | 수정 | esp_log.h include 제거 |
| display_hal | 수정 | esp_log.h include 제거 |

### 5.3 코딩 표준 준수

| 요소 | 언어 | 예시 |
|------|------|------|
| 로그 메시지 | **영문** | `T_LOGE(TAG, "Failed to initialize I2C: %d", err);` |
| 주석 | **한국어** | `// I2C 버스 초기화` |

---

*SPEC 버전: 1.0.0*
*생성일: 2026-01-15*
