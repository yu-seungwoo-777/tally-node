/**
 * @file button_poll.h
 * @brief BUTTON 0 폴링 버튼 감지 헤더
 * @author Claude Code
 * @date 2025-12-07
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
 * @brief BUTTON 폴링 초기화
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_poll_init(void);

/**
 * @brief BUTTON 폴링 태스크 시작
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_poll_start(void);

/**
 * @brief BUTTON 폴링 태스크 중지
 */
void button_poll_stop(void);

#include "button_actions.h"

/**
 * @brief 버튼 콜백 함수 타입
 */
typedef void (*button_callback_t)(button_action_t action);

/**
 * @brief 버튼 콜백 설정
 * @param callback 콜백 함수
 */
void button_poll_set_callback(button_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_POLL_H