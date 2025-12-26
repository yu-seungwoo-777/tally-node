/**
 * @file led_test.h
 * @brief WS2812 LED 테스트 앱
 */

#ifndef LED_TEST_H
#define LED_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 테스트 앱 초기화
 */
esp_err_t led_test_app_init(void);

/**
 * @brief LED 테스트 앱 정지
 */
void led_test_app_stop(void);

/**
 * @brief LED 테스트 앱 해제
 */
void led_test_app_deinit(void);

/**
 * @brief 1초마다 호출 - LED 상태 순환
 */
void led_test_app_tick(void);

#ifdef __cplusplus
}
#endif

#endif // LED_TEST_H
