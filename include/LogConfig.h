/**
 * @file LogConfig.h
 * @brief Tally Node 로그 설정
 *
 * 로그 시스템의 모든 설정 옵션을 관리
 */

#ifndef LOG_CONFIG_H
#define LOG_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 로그 레벨 설정
// ============================================================

/**
 * @brief 기본 로그 레벨
 * @note t_log.h의 T_LOG_DEFAULT_LEVEL보다 우선 적용
 * @note 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
 */
#ifndef T_LOG_DEFAULT_LEVEL
#define T_LOG_DEFAULT_LEVEL 3
#endif

// ============================================================
// 출력 형식 설정
// ============================================================

/**
 * @brief 타임스탬프 출력 여부
 * @define T_LOG_TIMESTAMP_ENABLE 1  타임스탬프 출력 (기본)
 * @define T_LOG_TIMESTAMP_ENABLE 0  타임스탬프 제거
 */
#ifndef T_LOG_TIMESTAMP_ENABLE
#define T_LOG_TIMESTAMP_ENABLE 1
#endif

/**
 * @brief 로그 레벨 문자 출력 여부
 * @define T_LOG_LEVEL_CHAR_ENABLE 1  레벨 문자 출력 I/E/W/D/V (기본)
 * @define T_LOG_LEVEL_CHAR_ENABLE 0  레벨 문자 제거
 */
#ifndef T_LOG_LEVEL_CHAR_ENABLE
#define T_LOG_LEVEL_CHAR_ENABLE 0
#endif

/**
 * @brief 태그 최대 길이 (괄호 포함)
 * @note 실제 태그는 TAG_MAX_LEN-2 까지 사용 가능, 초과 시 줄임표(...) 처리
 */
#define T_LOG_TAG_MAX_LEN 12

/**
 * @brief 로그 출력 버퍼 크기
 * @note 한 줄의 최대 길이 (넘으면 잘림)
 */
#define T_LOG_BUFFER_SIZE 256

// ============================================================
// 런타임 설정
// ============================================================

/**
 * @brief 로그 레벨 설정
 * @param level 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
 */
void t_log_set_level(int level);

/**
 * @brief 현재 로그 레벨 가져오기
 * @return 현재 로그 레벨 (0-5)
 */
int t_log_get_level(void);

/**
 * @brief 타임스탬프 출력 설정
 * @param enable 0=비활성, 1=활성
 */
void t_log_set_timestamp(int enable);

/**
 * @brief 타임스탬프 출력 상태 가져오기
 * @return 0=비활성, 1=활성
 */
int t_log_get_timestamp(void);

/**
 * @brief 로그 레벨 문자 출력 설정
 * @param enable 0=비활성, 1=활성
 */
void t_log_set_level_char(int enable);

/**
 * @brief 로그 레벨 문자 출력 상태 가져오기
 * @return 0=비활성, 1=활성
 */
int t_log_get_level_char(void);

// ============================================================
// 매크로 설정
// ============================================================

// 로그가 비활성화된 경우 모든 로그 매크로를 비활성화
#if T_LOG_DEFAULT_LEVEL == 0
#define T_LOG_ENABLED 0
#else
#define T_LOG_ENABLED 1
#endif

#ifdef __cplusplus
}
#endif

#endif // LOG_CONFIG_H
