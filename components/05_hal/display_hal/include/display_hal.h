/**
 * @file display_hal.h
 * @brief 디스플레이 HAL 인터페이스
 *
 * SSD1306 OLED 디스플레이를 위한 하드웨어 추상화 계층
 */

#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 공개 API
// ============================================================================

/**
 * @brief 디스플레이 HAL 초기화
 *
 * 디스플레이 HAL을 초기화합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t display_hal_init(void);

/**
 * @brief 디스플레이 HAL 해제
 */
void display_hal_deinit(void);

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool display_hal_is_initialized(void);

/**
 * @brief I2C 핀 번호 조회
 *
 * @param out_sda SDA 핀 번호를 저장할 포인터 (NULL 가능)
 * @param out_scl SCL 핀 번호를 저장할 포인터 (NULL 가능)
 */
void display_hal_get_i2c_pins(int *out_sda, int *out_scl);

/**
 * @brief I2C 포트 번호 조회
 *
 * @return I2C 포트 번호
 */
int display_hal_get_i2c_port(void);

/**
 * @brief 전원 상태 설정
 *
 * @param on true=켜기, false=끄기
 */
void display_hal_set_power(bool on);

/**
 * @brief 전원 상태 조회
 *
 * @return true 켜짐, false 꺼짐
 */
bool display_hal_get_power(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_HAL_H
