/**
 * @file switcher_dispatch.h
 * @brief Switcher Handler 타입 분기 매크로
 *
 * 반복되는 switch/case 패턴을 매크로로 추상화
 */

#ifndef SWITCHER_DISPATCH_H
#define SWITCHER_DISPATCH_H

#include "switcher.h"

/* ============================================================================
 * Protocol 클라이언트 포인터 얻기
 * ============================================================================ */

#define SWITCHER_GET_ATEM(sw) (&(sw)->backend.atem)
#define SWITCHER_GET_VMIX(sw) (&(sw)->backend.vmix)
#define SWITCHER_GET_OBS(sw) (&(sw)->backend.obs)

/* ============================================================================
 * 단순 분기 매크로 (반환값 int)
 * ============================================================================ */

/**
 * @brief 타입별 함수 호출 (반환값 int)
 *
 * @param sw switcher_t* 핸들
 * @param func_name 함수 이름 (atem_client_, vmix_client_, obs_client_ 접두어 제외)
 * @param ... 추가 인자
 *
 * 예: SWITCHER_DISPATCH_INT(sw, loop)
 *     → atem_client_loop(&sw->backend.atem)
 */
#define SWITCHER_DISPATCH_INT(sw, func_name, ...) \
    do { \
        switch ((sw)->type) { \
            SWITCHER_CASE_ATEM(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_VMIX(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_OBS(func_name, ##__VA_ARGS__) \
            default: \
                return SWITCHER_ERROR_NOT_SUPPORTED; \
        } \
    } while (0)

/* ATEM case */
#define SWITCHER_CASE_ATEM(func_name, ...) \
    case SWITCHER_TYPE_ATEM: \
        return atem_client_##func_name(SWITCHER_GET_ATEM(sw), ##__VA_ARGS__);

/* vMix case */
#define SWITCHER_CASE_VMIX(func_name, ...) \
    case SWITCHER_TYPE_VMIX: \
        return vmix_client_##func_name(SWITCHER_GET_VMIX(sw), ##__VA_ARGS__);

/* OBS case */
#define SWITCHER_CASE_OBS(func_name, ...) \
    case SWITCHER_TYPE_OBS: \
        return obs_client_##func_name(SWITCHER_GET_OBS(sw), ##__VA_ARGS__);

/* ============================================================================
 * uint8_t 반환 분기 매크로
 * ============================================================================ */

#define SWITCHER_DISPATCH_UINT8(sw, func_name, ...) \
    do { \
        switch ((sw)->type) { \
            SWITCHER_CASE_ATEM_UINT8(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_VMIX_UINT8(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_OBS_UINT8(func_name, ##__VA_ARGS__) \
            default: \
                return 0; \
        } \
    } while (0)

#define SWITCHER_CASE_ATEM_UINT8(func_name, ...) \
    case SWITCHER_TYPE_ATEM: \
        return atem_client_##func_name(SWITCHER_GET_ATEM(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_VMIX_UINT8(func_name, ...) \
    case SWITCHER_TYPE_VMIX: \
        return vmix_client_##func_name(SWITCHER_GET_VMIX(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_OBS_UINT8(func_name, ...) \
    case SWITCHER_TYPE_OBS: \
        return obs_client_##func_name(SWITCHER_GET_OBS(sw), ##__VA_ARGS__);

/* ============================================================================
 * uint16_t 반환 분기 매크로
 * ============================================================================ */

#define SWITCHER_DISPATCH_UINT16(sw, func_name, ...) \
    do { \
        switch ((sw)->type) { \
            SWITCHER_CASE_ATEM_UINT16(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_VMIX_UINT16(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_OBS_UINT16(func_name, ##__VA_ARGS__) \
            default: \
                return 0; \
        } \
    } while (0)

#define SWITCHER_CASE_ATEM_UINT16(func_name, ...) \
    case SWITCHER_TYPE_ATEM: \
        return atem_client_##func_name(SWITCHER_GET_ATEM(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_VMIX_UINT16(func_name, ...) \
    case SWITCHER_TYPE_VMIX: \
        return vmix_client_##func_name(SWITCHER_GET_VMIX(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_OBS_UINT16(func_name, ...) \
    case SWITCHER_TYPE_OBS: \
        return obs_client_##func_name(SWITCHER_GET_OBS(sw), ##__VA_ARGS__);

/* ============================================================================
 * uint64_t 반환 분기 매크로
 * ============================================================================ */

#define SWITCHER_DISPATCH_UINT64(sw, func_name, ...) \
    do { \
        switch ((sw)->type) { \
            SWITCHER_CASE_ATEM_UINT64(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_VMIX_UINT64(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_OBS_UINT64(func_name, ##__VA_ARGS__) \
            default: \
                return 0; \
        } \
    } while (0)

#define SWITCHER_CASE_ATEM_UINT64(func_name, ...) \
    case SWITCHER_TYPE_ATEM: \
        return atem_client_##func_name(SWITCHER_GET_ATEM(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_VMIX_UINT64(func_name, ...) \
    case SWITCHER_TYPE_VMIX: \
        return vmix_client_##func_name(SWITCHER_GET_VMIX(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_OBS_UINT64(func_name, ...) \
    case SWITCHER_TYPE_OBS: \
        return obs_client_##func_name(SWITCHER_GET_OBS(sw), ##__VA_ARGS__);

/* ============================================================================
 * bool 반환 분기 매크로
 * ============================================================================ */

#define SWITCHER_DISPATCH_BOOL(sw, func_name, ...) \
    do { \
        switch ((sw)->type) { \
            SWITCHER_CASE_ATEM_BOOL(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_VMIX_BOOL(func_name, ##__VA_ARGS__) \
            SWITCHER_CASE_OBS_BOOL(func_name, ##__VA_ARGS__) \
            default: \
                return false; \
        } \
    } while (0)

#define SWITCHER_CASE_ATEM_BOOL(func_name, ...) \
    case SWITCHER_TYPE_ATEM: \
        return atem_client_##func_name(SWITCHER_GET_ATEM(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_VMIX_BOOL(func_name, ...) \
    case SWITCHER_TYPE_VMIX: \
        return vmix_client_##func_name(SWITCHER_GET_VMIX(sw), ##__VA_ARGS__);

#define SWITCHER_CASE_OBS_BOOL(func_name, ...) \
    case SWITCHER_TYPE_OBS: \
        return obs_client_##func_name(SWITCHER_GET_OBS(sw), ##__VA_ARGS__);

#endif /* SWITCHER_DISPATCH_H */