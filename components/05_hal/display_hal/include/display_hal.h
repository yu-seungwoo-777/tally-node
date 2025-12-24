/**
 * @file display_hal.h
 * @brief 디스플레이 HAL (I2C + U8g2)
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

/**
 * @brief 디스플레이 HAL 초기화
 *
 * I2C 마스터를 초기화하고 U8g2 HAL을 설정합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t display_hal_init(void);

/**
 * @brief I2C 핀 설정 가져오기
 *
 * @param out_sda SDA 핀 번호 출력 포인터
 * @param out_scl SCL 핀 번호 출력 포인터
 */
void display_hal_get_i2c_pins(int *out_sda, int *out_scl);

/**
 * @brief I2C 포트 번호 가져오기
 *
 * @return I2C 포트 번호 (I2C_NUM_0 또는 I2C_NUM_1)
 */
int display_hal_get_i2c_port(void);

/**
 * @brief 디스플레이 전원 켜기/끄기
 *
 * @param on true: 켜기, false: 끄기
 */
void display_hal_set_power(bool on);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_HAL_H
