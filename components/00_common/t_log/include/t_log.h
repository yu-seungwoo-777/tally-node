/**
 * @file t_log.h
 * @brief Tally Node 전용 로그 시스템
 *
 * ESP 로그 대체 - LogConfig.h에서 설정 관리
 */

#ifndef T_LOG_H
#define T_LOG_H

#include <stdio.h>
#include "LogConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 로그 레벨
// ============================================================

typedef enum {
    T_LOG_NONE    = 0,  ///< 로그 비활성화
    T_LOG_ERROR   = 1,  ///< 에러
    T_LOG_WARN    = 2,  ///< 경고
    T_LOG_INFO    = 3,  ///< 정보
    T_LOG_DEBUG   = 4,  ///< 디버그
    T_LOG_VERBOSE = 5,  ///< 상세
    T_LOG_MAX        ///< 최대 레벨
} t_log_level_t;

// ============================================================
// 내부 함수
// ============================================================

/**
 * @brief 타임스탬프 가져오기
 */
uint32_t t_log_timestamp(void);

/**
 * @brief 내부 로그 출력 함수
 */
void t_log_output(t_log_level_t level, const char* tag, const char* fmt, ...);

// ============================================================
// 로그 매크로
// ============================================================

#define T_LOGE(tag, fmt, ...) do { \
    if (T_LOG_ERROR <= T_LOG_DEFAULT_LEVEL) { \
        t_log_output(T_LOG_ERROR, tag, fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define T_LOGW(tag, fmt, ...) do { \
    if (T_LOG_WARN <= T_LOG_DEFAULT_LEVEL) { \
        t_log_output(T_LOG_WARN, tag, fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define T_LOGI(tag, fmt, ...) do { \
    if (T_LOG_INFO <= T_LOG_DEFAULT_LEVEL) { \
        t_log_output(T_LOG_INFO, tag, fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define T_LOGD(tag, fmt, ...) do { \
    if (T_LOG_DEBUG <= T_LOG_DEFAULT_LEVEL) { \
        t_log_output(T_LOG_DEBUG, tag, fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define T_LOGV(tag, fmt, ...) do { \
    if (T_LOG_VERBOSE <= T_LOG_DEFAULT_LEVEL) { \
        t_log_output(T_LOG_VERBOSE, tag, fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

// ============================================================
// ESP_ERROR_CHECK 호환
// ============================================================

#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x) do { \
    esp_err_t err_rc = (x); \
    if (unlikely(err_rc != ESP_OK)) { \
        T_LOGE(ERR, "ESP_ERROR_CHECK failed: %s:%d - err=0x%x", __FILE__, __LINE__, err_rc); \
        abort(); \
    } \
} while(0)
#endif

// likely/unlikely 정의
#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

// ============================================================
// Hex Dump Macros (ESP_LOG_BUFFER_HEXDUMP 대체)
// ============================================================

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

// 편의 매크로 (레벨별)
#define T_LOGE_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_ERROR, tag, data, len)
#define T_LOGW_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_WARN, tag, data, len)
#define T_LOGI_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_INFO, tag, data, len)
#define T_LOGD_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_DEBUG, tag, data, len)
#define T_LOGV_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_VERBOSE, tag, data, len)

#ifdef __cplusplus
}
#endif

#endif // T_LOG_H
