/**
 * @file ws2812_driver.h
 * @brief WS2812 Driver - RGB 변환, 상태 관리
 */

#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WS2812 LED 상태
 */
typedef enum {
    WS2812_OFF = 0,        ///< 꺼짐
    WS2812_PROGRAM = 1,     ///< 빨강 (PGM)
    WS2812_PREVIEW = 2,     ///< 초록 (PV)
    WS2812_LIVE = 3,        ///< 파랑 (LIVE)
    WS2812_BATTERY_LOW = 4, ///< 노랑 (배터리 경고)
} ws2812_state_t;

/**
 * @brief WS2812 드라이버 초기화
 * @param gpio_num GPIO 핀 번호 (-1=PinConfig.h 사용)
 * @param num_leds LED 개수 (0=드라이버 기본값 사용)
 * @param camera_id 카메라 ID (0=기본값 1)
 * @return ESP_OK 성공
 */
esp_err_t ws2812_driver_init(int gpio_num, uint32_t num_leds, uint8_t camera_id);

/**
 * @brief WS2812 상태 설정 (모든 LED)
 * @param state LED 상태
 */
void ws2812_set_state(ws2812_state_t state);

/**
 * @brief WS2812 RGB 직접 설정 (모든 LED)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief WS2812 개별 LED 상태 설정
 * @param led_index LED 인덱스 (0부터)
 * @param state LED 상태
 */
void ws2812_set_led_state(uint32_t led_index, ws2812_state_t state);

/**
 * @brief WS2812 개별 LED RGB 설정
 * @param led_index LED 인덱스 (0부터)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void ws2812_set_led_rgb(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief WS2812 밝기 설정
 * @param brightness 밝기 (0-255)
 */
void ws2812_set_brightness(uint8_t brightness);

/**
 * @brief 카메라 ID 설정 (Tally 이벤트 수신 시 사용)
 * @param camera_id 카메라 ID (1-255)
 */
void ws2812_set_camera_id(uint8_t camera_id);

/**
 * @brief Tally 데이터 처리 (service에서 호출)
 * @param tally_data Tally 데이터 배열
 * @param channel_count 채널 수
 */
void ws2812_process_tally_data(const uint8_t* tally_data, uint8_t channel_count);

/**
 * @brief WS2812 모든 LED 끄기
 */
void ws2812_off(void);

/**
 * @brief WS2812 해제
 */
void ws2812_deinit(void);

/**
 * @brief 초기화 여부
 */
bool ws2812_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // WS2812_DRIVER_H
