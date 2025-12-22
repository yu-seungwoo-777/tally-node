/**
 * @file switcher_types.h
 * @brief Switcher 공통 타입 정의
 *
 * 순환 의존성 방지를 위해 switcher_type_t를 별도 헤더로 분리
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 스위처 타입
 */
typedef enum {
    SWITCHER_TYPE_UNKNOWN = 0,
    SWITCHER_TYPE_ATEM,       /**< Blackmagic Design ATEM */
    SWITCHER_TYPE_VMIX,       /**< vMix */
    SWITCHER_TYPE_OBS,        /**< OBS Studio */
    SWITCHER_TYPE_OSEE,       /**< OSEE */
    SWITCHER_TYPE_MAX
} switcher_type_t;

/**
 * @brief 스위처 조회 모드
 *
 * - SINGLE: 하나의 스위처만 사용 (기본)
 * - DUAL: 두 개의 스위처를 조합하여 사용 (카메라 번호 매핑)
 */
typedef enum {
    SWITCHER_MODE_SINGLE = 0,    /**< 싱글 모드 (1개 스위처) */
    SWITCHER_MODE_DUAL   = 1     /**< 듀얼 모드 (2개 스위처 조합) */
} switcher_mode_t;

#ifdef __cplusplus
}
#endif
