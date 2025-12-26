/**
 * @file temperature_hal.h
 * @brief 온도 센서 HAL (ESP32-S3 내장 온도 센서)
 */

#ifndef TEMPERATURE_HAL_H
#define TEMPERATURE_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 온도 센서 초기화
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_init(void);

/**
 * @brief 온도 센서 해제
 */
void temperature_hal_deinit(void);

/**
 * @brief 온도 측정 (섭씨)
 *
 * @param temp_c 온도값(섭씨)을 저장할 포인터
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_read_celsius(float* temp_c);

/**
 * @brief 온도 측정 (화씨)
 *
 * @param temp_f 온도값(화씨)을 저장할 포인터
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_read_fahrenheit(float* temp_f);

#ifdef __cplusplus
}
#endif

#endif // TEMPERATURE_HAL_H
