/**
 * @file LedService.h
 * @brief LED 서비스 - ConfigService 색상을 WS2812Driver에 적용
 */

#ifndef LED_SERVICE_H
#define LED_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 서비스 초기화
 * @param gpio_num GPIO 핀 번호 (-1=PinConfig.h 사용)
 * @param num_leds LED 개수 (0=기본값 8)
 * @param camera_id 카메라 ID (0=ConfigService에서 로드)
 * @return ESP_OK 성공
 */
esp_err_t led_service_init(int gpio_num, uint32_t num_leds, uint8_t camera_id);

/**
 * @brief LED 상태 설정 (WS2812Driver 위임)
 * @param state LED 상태 (WS2812_PROGRAM/PREVIEW/OFF/BATTERY_LOW)
 */
void led_service_set_state(int state);

/**
 * @brief LED RGB 직접 설정 (WS2812Driver 위임)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void led_service_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief LED 밝기 설정 (WS2812Driver 위임)
 * @param brightness 밝기 (0-255)
 */
void led_service_set_brightness(uint8_t brightness);

/**
 * @brief 카메라 ID 설정 (WS2812Driver 위임)
 * @param camera_id 카메라 ID (1-255)
 */
void led_service_set_camera_id(uint8_t camera_id);

/**
 * @brief LED 모두 끄기 (WS2812Driver 위임)
 */
void led_service_off(void);

/**
 * @brief LED 서비스 해제
 */
void led_service_deinit(void);

/**
 * @brief 초기화 여부
 */
bool led_service_is_initialized(void);

/**
 * @brief 색상 ConfigService에서 다시 로드하여 WS2812Driver에 적용
 */
void led_service_load_colors(void);

#ifdef __cplusplus
}
#endif

#endif // LED_SERVICE_H
