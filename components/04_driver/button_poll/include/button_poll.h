/**
 * @file button_poll.h
 * @brief 버튼 폴링 컴포넌트
 *
 * - 단일 클릭 (Single Click)
 * - 롱 프레스 (Long Press, 1000ms)
 * - 롱 프레스 해제 (Long Release)
 */

#ifndef BUTTON_POLL_H
#define BUTTON_POLL_H

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
    BUTTON_ACTION_SINGLE = 1,      /**< 단일 클릭 */
    BUTTON_ACTION_LONG   = 99,     /**< 롱 프레스 (1000ms) */
    BUTTON_ACTION_LONG_RELEASE = 98 /**< 롱 프레스 해제 */
} button_action_t;

/**
 * @brief 버튼 콜백 함수 타입
 *
 * @param action 발생한 버튼 액션
 */
typedef void (*button_callback_t)(button_action_t action);

/**
 * @brief 버튼 폴링 초기화
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_poll_init(void);

/**
 * @brief 버튼 폴링 태스크 시작
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_poll_start(void);

/**
 * @brief 버튼 폴링 태스크 중지
 */
void button_poll_stop(void);

/**
 * @brief 버튼 폴링 해제
 */
void button_poll_deinit(void);

/**
 * @brief 버튼 콜백 설정
 *
 * @param callback 콜백 함수 (NULL로 설정 시 콜백 비활성화)
 */
void button_poll_set_callback(button_callback_t callback);

/**
 * @brief 버튼 현재 상태 확인
 *
 * @return true 버튼 눌림, false 버튼 안 눌림
 */
bool button_poll_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_POLL_H
