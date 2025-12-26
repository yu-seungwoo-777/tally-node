/**
 * @file battery_hal.h
 * @brief 배터리 전압 측정 HAL
 */

#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 배터리 HAL 초기화
 */
esp_err_t battery_hal_init(void);

/**
 * @brief 배터리 전압 읽기 (V)
 * @param voltage 출력 전압 (V)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t battery_hal_read_voltage(float* voltage);

/**
 * @brief 배터리 전압 밀리볼트로 읽기
 * @param voltage_mv 출력 전압 (mV)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t battery_hal_read_voltage_mV(int* voltage_mv);

/**
 * @brief 초기화 여부
 */
bool battery_hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_HAL_H
