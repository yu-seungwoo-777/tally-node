/**
 * @file button_handler.h
 * @brief 버튼 핸들러 (디스플레이 페이지 전환)
 *
 * button_poll 서비스의 이벤트를 받아서
 * DisplayManager의 페이지 전환 기능을 수행합니다.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 버튼 핸들러 초기화
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_handler_init(void);

/**
 * @brief 버튼 핸들러 시작
 *
 * button_poll에 콜백을 등록합니다.
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_handler_start(void);

/**
 * @brief 버튼 핸들러 중지
 */
void button_handler_stop(void);

/**
 * @brief 버튼 핸들러 해제
 */
void button_handler_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_HANDLER_H
