/**
 * @file LedService.h
 * @brief LED 서비스 - WS2812Driver + 내장 LED 제어
 */

#ifndef LED_SERVICE_H
#define LED_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "board_led_driver.h"  // board_led_state_t 사용

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 색상 설정 구조체
 */
typedef struct {
    uint8_t program_r, program_g, program_b;
    uint8_t preview_r, preview_g, preview_b;
    uint8_t off_r, off_g, off_b;
    uint8_t battery_low_r, battery_low_g, battery_low_b;
} led_colors_t;

/**
 * @brief LED 서비스 초기화
 * @param gpio_num GPIO 핀 번호 (-1=PinConfig.h 사용)
 * @param num_leds LED 개수 (0=기본값 8)
 * @param camera_id 카메라 ID
 * @return ESP_OK 성공
 */
esp_err_t led_service_init(int gpio_num, uint32_t num_leds, uint8_t camera_id);

/**
 * @brief LED 서비스 초기화 (색상 포함)
 * @param gpio_num GPIO 핀 번호
 * @param num_leds LED 개수
 * @param camera_id 카메라 ID
 * @param colors 색상 설정 (NULL=기본값 사용)
 * @return ESP_OK 성공
 */
esp_err_t led_service_init_with_colors(int gpio_num, uint32_t num_leds, uint8_t camera_id, const led_colors_t* colors);

/**
 * @brief LED 색상 설정
 * @param colors 색상 설정
 * @return ESP_OK 성공
 */
esp_err_t led_service_set_colors(const led_colors_t* colors);

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

// ============================================================================
// 내장 LED 제어 (board_led_driver 위임)
// ============================================================================

/**
 * @brief 내장 LED 초기화
 * @return ESP_OK 성공
 */
esp_err_t led_service_init_board_led(void);

/**
 * @brief 내장 LED 해제
 */
void led_service_deinit_board_led(void);

/**
 * @brief 내장 LED 상태 설정
 * @param state ON/OFF 상태
 */
void led_service_set_board_led_state(board_led_state_t state);

/**
 * @brief 내장 LED 켜기
 */
void led_service_board_led_on(void);

/**
 * @brief 내장 LED 끄기
 */
void led_service_board_led_off(void);

/**
 * @brief 내장 LED 토글
 */
void led_service_toggle_board_led(void);

#ifdef __cplusplus
}
#endif

#endif // LED_SERVICE_H
