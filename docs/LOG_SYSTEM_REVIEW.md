# Log System Review

**Date:** 2026-01-15
**Status:** Draft

## Overview

Tally Node 프로젝트는 ESP-IDF 기본 로그 시스템(`esp_log.h`)을 대체하여 커스텀 `t_log` 시스템을 사용합니다. 본 문서는 ESP_LOG 잔존 여부를 리뷰한 결과를 기록합니다.

---

## Summary

- **ESP_LOG 잔존:** 2개소 (`u8g2_esp32_hal.c`)
- **esp_log.h include:** 11개 파일 (그중 1개에서 사용, 10개는 미사용)
- **t_log 대체 완료:** 대부분의 코드

---

## ESP_LOG 사용 현황

### 1. u8g2_esp32_hal.c

| 위치 | 라인 | 코드 | 내용 |
|------|------|------|------|
| `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c` | 27 | `ESP_LOGE("err", "esp_err_t = %d", rc)` | ESP_ERROR_CHECK 매크로 재정의 내부 |
| `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c` | 142 | `ESP_LOG_BUFFER_HEXDUMP(TAG, data_ptr, arg_int, ESP_LOG_VERBOSE)` | I2C 데이터 hex dump |

### 상세 코드

#### 1) ESP_ERROR_CHECK 매크로 (라인 22-30)

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

**문제점:**
- `esp_log.h`를 include하고 ESP_LOGE를 직접 사용
- t_log.h에서 제공하는 ESP_ERROR_CHECK 매크로가 있음에도 중복 정의

**t_log.h의 ESP_ERROR_CHECK (라인 86-94):**
```c
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

#### 2) ESP_LOG_BUFFER_HEXDUMP (라인 142)

```c
case U8X8_MSG_BYTE_SEND: {
  uint8_t* data_ptr = (uint8_t*)arg_ptr;
  ESP_LOG_BUFFER_HEXDUMP(TAG, data_ptr, arg_int, ESP_LOG_VERBOSE);
  // ...
```

**문제점:**
- I2C 전송 데이터 디버깅용 hex dump
- t_log 시스템에 이에 대응하는 매크로 없음

---

## esp_log.h Include 현황

### 사용 중 (1개 파일)

| 파일 | 사용 매크로 |
|------|-------------|
| `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c` | ESP_LOGE, ESP_LOG_BUFFER_HEXDUMP |

### 미사용 (10개 파일)

다음 파일에서 `esp_log.h`를 include하나 실제 ESP_LOG 매크로는 사용하지 않음:

| 파일 |
|------|
| `components/02_presentation/web_server/web_server_config.cpp` |
| `components/02_presentation/web_server/web_server_events.cpp` |
| `components/02_presentation/web_server/web_server_json.cpp` |
| `components/02_presentation/web_server/handlers/api_status.cpp` |
| `components/02_presentation/web_server/handlers/api_license.cpp` |
| `components/02_presentation/web_server/handlers/api_devices.cpp` |
| `components/02_presentation/web_server/handlers/api_lora.cpp` |
| `components/05_hal/battery_hal/battery_hal.c` |
| `components/05_hal/ws2812_hal/ws2812_hal.c` |
| `components/05_hal/display_hal/display_hal.c` |

**권장사항:** 사용하지 않는 include 제거

---

## t_log 시스템

### 제공 매크로

| 매크로 | 설명 |
|--------|------|
| `T_LOGE(tag, fmt, ...)` | Error |
| `T_LOGW(tag, fmt, ...)` | Warning |
| `T_LOGI(tag, fmt, ...)` | Info |
| `T_LOGD(tag, fmt, ...)` | Debug |
| `T_LOGV(tag, fmt, ...)` | Verbose |

### 설정 (LogConfig.h)

- `T_LOG_DEFAULT_LEVEL`: 로그 레벨 (0=None ~ 5=Verbose)
- `T_LOG_TIMESTAMP_ENABLE`: 타임스탬프 출력
- `T_LOG_LEVEL_CHAR_ENABLE`: 레벨 문자 출력
- `T_LOG_RAM_INFO_INTERVAL_MS`: RAM 사용량 출력 주기

### 제공 기능

| 함수 | 설명 |
|------|------|
| `t_log_set_level(int level)` | 런타임 로그 레벨 설정 |
| `t_log_get_level(void)` | 현재 로그 레벨 가져오기 |
| `t_log_set_timestamp(int enable)` | 타임스탬프 출력 설정 |
| `t_log_get_timestamp(void)` | 타임스탬프 상태 가져오기 |
| `t_log_set_level_char(int enable)` | 레벨 문자 출력 설정 |
| `t_log_get_level_char(void)` | 레벨 문자 상태 가져오기 |

---

## Action Items

### Priority 1: ESP_LOG 제거

1. **u8g2_esp32_hal.c ESP_ERROR_CHECK 수정**
   - 커스텀 ESP_ERROR_CHECK 매크로 삭제
   - t_log.h의 ESP_ERROR_CHECK 사용 (include만 추가)

2. **ESP_LOG_BUFFER_HEXDUMP 대안**
   - t_log에 hex dump 매크로 추가
   - 또는 해당 로그를 조건부 컴파일로 비활성화

### Priority 2: 불필요한 include 제거

- 10개 파일의 `#include "esp_log.h"` 제거
- (u8g2_esp32_hal.c는 수정 후 제거)

---

## 제안: t_log Hex Dump 매크로

```c
// t_log.h에 추가 제안

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

// 사용 편의 매크로
#define T_LOGE_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_ERROR, tag, data, len)
#define T_LOGW_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_WARN, tag, data, len)
#define T_LOGI_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_INFO, tag, data, len)
#define T_LOGD_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_DEBUG, tag, data, len)
#define T_LOGV_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_VERBOSE, tag, data, len)
```

---

## References

- `include/LogConfig.h` - 로그 설정
- `components/00_common/t_log/include/t_log.h` - t_log 인터페이스
- `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c` - ESP_LOG 사용 파일
