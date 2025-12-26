/**
 * @file ws2812_hal.h
 * @brief WS2812 HAL - RMT 하드웨어 제어
 */

#ifndef WS2812_HAL_H
#define WS2812_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WS2812 HAL 초기화
 * @param gpio_num WS2812 데이터 GPIO 핀
 * @param num_leds LED 개수
 * @return ESP_OK 성공
 */
esp_err_t ws2812_hal_init(int gpio_num, uint32_t num_leds);

/**
 * @brief WS2812 데이터 전송 (GRB格式)
 * @param data RGB 데이터 (GRB 순서, LED 개수 * 3 바이트)
 * @param length 데이터 길이
 * @return ESP_OK 성공
 */
esp_err_t ws2812_hal_transmit(const uint8_t* data, size_t length);

/**
 * @brief WS2812 초기화 해제
 */
void ws2812_hal_deinit(void);

/**
 * @brief 초기화 여부
 */
bool ws2812_hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // WS2812_HAL_H
