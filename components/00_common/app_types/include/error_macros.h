#ifndef ERROR_MACROS_H
#define ERROR_MACROS_H

#include "esp_err.h"
#include "t_log.h"

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
