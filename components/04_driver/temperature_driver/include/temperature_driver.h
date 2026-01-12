/**
 * @file TemperatureDriver.h
 * @brief 온도 센서 드라이버 (C++)
 *
 * ESP32-S3 내장 온도 센서 드라이버
 */

#ifndef TEMPERATURE_DRIVER_H
#define TEMPERATURE_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 온도 센서 드라이버 초기화
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_driver_init(void);

/**
 * @brief 온도 센서 드라이버 해제
 */
void temperature_driver_deinit(void);

/**
 * @brief 온도 측정 (섭씨)
 *
 * @param temp_c 온도값(섭씨)을 저장할 포인터
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_driver_get_celsius(float* temp_c);

/**
 * @brief 온도 측정 (화씨)
 *
 * @param temp_f 온도값(화씨)을 저장할 포인터
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_driver_get_fahrenheit(float* temp_f);

#ifdef __cplusplus
}
#endif

#endif // TEMPERATURE_DRIVER_H
