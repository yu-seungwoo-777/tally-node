#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 로그 레벨 정의 (0, 1 만 지원)
 */
typedef enum {
    LOG_LEVEL_0 = 0,  // 기본 로그 (항상 출력)
    LOG_LEVEL_1 = 1,  // 상세 로그 (디버그용)
} log_level_t;

/**
 * @brief 로그 시스템 초기화
 *
 * @param default_level 기본 로그 레벨 (LOG_LEVEL_0 또는 LOG_LEVEL_1)
 */
void log_init(log_level_t default_level);

/**
 * @brief 현재 로그 레벨 설정
 *
 * @param level 설정할 로그 레벨
 */
void log_set_level(log_level_t level);

/**
 * @brief 현재 로그 레벨 가져오기
 *
 * @return log_level_t 현재 로그 레벨
 */
log_level_t log_get_level(void);

/**
 * @brief 로그 출력 (레벨 0 - 기본)
 *
 * @param tag 로그 태그
 * @param format 포맷 문자열
 * @param ... 가변 인자
 */
void log_0(const char *tag, const char *format, ...);

/**
 * @brief 로그 출력 (레벨 1 - 상세)
 *
 * @param tag 로그 태그
 * @param format 포맷 문자열
 * @param ... 가변 인자
 */
void log_1(const char *tag, const char *format, ...);

/**
 * @brief 로그 버퍼 비우기 (즉시 출력 보장)
 */
void log_flush(void);

// 편의 매크로
#define LOG_0(tag, ...) log_0(tag, __VA_ARGS__)
#define LOG_1(tag, ...) log_1(tag, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOG_H
