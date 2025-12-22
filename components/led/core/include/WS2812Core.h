/**
 * @file WS2812Core.h
 * @brief WS2812 LED 제어 (ESP-IDF RMT 드라이버 사용)
 */

#ifndef WS2812_CORE_H
#define WS2812_CORE_H

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
    WS2812_OFF = 0,      ///< LED 꺼짐
    WS2812_PROGRAM = 1,  ///< 빨간색 (Program)
    WS2812_PREVIEW = 2   ///< 초록색 (Preview)
} ws2812_state_t;

/**
 * @brief WS2812 초기화
 *
 * @param gpio_num WS2812 데이터 GPIO 핀 번호
 * @param num_leds LED 개수
 * @return esp_err_t ESP_OK 성공, 그외 에러 코드
 */
esp_err_t WS2812Core_init(int gpio_num, uint32_t num_leds);

/**
 * @brief WS2812 초기화 (PinConfig.h 핀 사용)
 *
 * @return esp_err_t ESP_OK 성공, 그외 에러 코드
 */
esp_err_t WS2812Core_initDefault(void);

/**
 * @brief WS2812 LED 상태 설정 (모든 LED 동일)
 *
 * @param state 설정할 LED 상태
 */
void WS2812Core_setState(ws2812_state_t state);

/**
 * @brief WS2812 개별 LED 상태 설정
 *
 * @param led_index LED 인덱스 (0부터 시작)
 * @param state 설정할 LED 상태
 */
void WS2812Core_setLedState(uint32_t led_index, ws2812_state_t state);

/**
 * @brief WS2812 여러 LED 상태 한 번에 설정
 *
 * @param states LED 상태 배열
 * @param count LED 개수
 */
void WS2812Core_setLedStates(const ws2812_state_t* states, uint32_t count);

/**
 * @brief WS2812 LED 끄기 (모든 LED)
 */
void WS2812Core_off(void);

/**
 * @brief WS2812 개별 LED 끄기
 *
 * @param led_index LED 인덱스
 */
void WS2812Core_setLedOff(uint32_t led_index);

/**
 * @brief WS2812 LED 밝기 설정
 *
 * @param brightness 밝기 (1-255)
 */
void WS2812Core_setBrightness(uint8_t brightness);

/**
 * @brief WS2812 초기화 해제
 */
void WS2812Core_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WS2812_CORE_H
