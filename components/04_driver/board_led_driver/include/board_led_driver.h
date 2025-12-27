/**
 * @file board_led_driver.h
 * @brief 내장 LED 드라이버 (GPIO 37)
 */

#ifndef BOARD_LED_DRIVER_H
#define BOARD_LED_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 내장 LED 상태
 */
typedef enum {
    BOARD_LED_OFF = 0,
    BOARD_LED_ON = 1,
} board_led_state_t;

/**
 * @brief 내장 LED 드라이버 초기화
 * @return ESP_OK 성공
 */
esp_err_t board_led_driver_init(void);

/**
 * @brief 내장 LED 상태 설정
 * @param state ON/OFF 상태
 */
void board_led_set_state(board_led_state_t state);

/**
 * @brief 내장 LED 켜기
 */
void board_led_on(void);

/**
 * @brief 내장 LED 끄기
 */
void board_led_off(void);

/**
 * @brief 내장 LED 토글
 */
void board_led_toggle(void);

/**
 * @brief 내장 LED 해제
 */
void board_led_driver_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BOARD_LED_DRIVER_H
