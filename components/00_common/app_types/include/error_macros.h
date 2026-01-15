#ifndef ERROR_MACROS_H
#define ERROR_MACROS_H

#include "esp_err.h"
#include "t_log.h"

// ============================================================================
// esp_err_t 반환 함수용 매크로
// ============================================================================

// esp_err_t 반환 함수 체인용
#define RETURN_IF_ERROR(x) do { \
    esp_err_t _err = (x); \
    if (_err != ESP_OK) return _err; \
} while(0)

// 조건부 에러 반환
#define RETURN_ERROR_IF(cond, err) do { \
    if (cond) return (err); \
} while(0)

// esp_err_t 반환 함수용 NULL 체크
#define RETURN_ERR_IF_NULL(ptr) do { \
    if (!(ptr)) { \
        T_LOGE(TAG, "NULL argument: %s", #ptr); \
        return ESP_ERR_INVALID_ARG; \
    } \
} while(0)

// 초기화 상태 체크 (s_initialized 등)
#define RETURN_ERR_IF_NOT_INIT(init_flag) do { \
    if (!(init_flag)) { \
        return ESP_ERR_INVALID_STATE; \
    } \
} while(0)

// ============================================================================
// bool 반환 함수용 매크로
// ============================================================================

// bool 반환 함수용 NULL 체크
#define RETURN_BOOL_IF_NULL(ptr) do { \
    if (!(ptr)) { \
        return false; \
    } \
} while(0)

// ============================================================================
// void 반환 함수용 매크로 (side-effect only)
// ============================================================================

// void 함수용 NULL 체크 (로그만 출력)
#define CHECK_NULL(ptr) do { \
    if (!(ptr)) { \
        T_LOGE(TAG, "NULL argument: %s", #ptr); \
        return; \
    } \
} while(0)

// ============================================================================
// Driver/HAL 공통 상태 체크 매크로
// ============================================================================

// 중복 초기화 방지
#define CHECK_NOT_INITIALIZED(init_flag) do { \
    if (init_flag) { \
        return ESP_ERR_INVALID_STATE; \
    } \
} while(0)

// 초기화되지 않음 방지
#define CHECK_INITIALIZED(init_flag) do { \
    if (!(init_flag)) { \
        return ESP_ERR_INVALID_STATE; \
    } \
} while(0)

#endif // ERROR_MACROS_H
