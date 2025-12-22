/**
 * @file button_actions.h
 * @brief 버튼 액션 맵핑 정의
 * @author Claude Code
 * @date 2025-12-07
 */

#ifndef BUTTON_ACTIONS_H
#define BUTTON_ACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 버튼 액션 타입
 */
typedef enum {
    BUTTON_ACTION_SINGLE = 1,     // 1회 클릭
    BUTTON_ACTION_LONG   = 99,    // 롱프레스
    BUTTON_ACTION_LONG_RELEASE = 98  // 롱프레스 해제
} button_action_t;

/**
 * @brief 버튼 기능 정보 구조체
 */
typedef struct {
    const char* name;          // 기능 이름
    const char* description;   // 기능 설명
    void (*action)(void);      // 실행 함수
} button_function_t;

/**
 * @brief 버튼 액션 핸들러 함수 포인터
 */
typedef void (*button_action_handler_t)(button_action_t action);

/**
 * @brief 버튼 액션 초기화
 *
 * TX/RX 모드에 따라 적절한 기능 테이블을 설정합니다.
 */
void button_actions_init(void);

/**
 * @brief 버튼 액션 실행
 *
 * @param action 버튼 액션 타입
 */
void button_actions_execute(button_action_t action);

/**
 * @brief 버튼 액션 핸들러 설정
 *
 * @param handler 액션 핸들러 함수
 */
void button_actions_set_handler(button_action_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_ACTIONS_H