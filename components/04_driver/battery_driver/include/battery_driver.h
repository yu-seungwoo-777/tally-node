/**
 * @file battery_driver.h
 * @brief 배터리 드라이버 (전압 → 백분율 변환)
 */

#ifndef BATTERY_DRIVER_H
#define BATTERY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 배터리 상태 구조체 (전압 + 백분율)
 */
typedef struct {
    float voltage;   ///< 배터리 전압 (V)
    uint8_t percent; ///< 배터리 백분율 (0~100%)
} battery_status_t;

/**
 * @brief 배터리 드라이버 초기화
 */
esp_err_t battery_driver_init(void);

/**
 * @brief 배터리 전압 읽기 (V)
 * @param voltage 출력 전압
 * @return ESP_OK 성공
 */
esp_err_t battery_driver_get_voltage(float* voltage);

/**
 * @brief 배터리 백분율 읽기
 * @return 0-100 백분율
 */
uint8_t battery_driver_get_percent(void);

/**
 * @brief 배터리 백분율 업데이트 및 반환
 * @return 업데이트된 백분율 (0-100)
 */
uint8_t battery_driver_update_percent(void);

/**
 * @brief 배터리 상태 업데이트 (전압 + 백분율, ADC 1회만 읽기)
 *
 * 중복 ADC 읽기를 방지하기 위해 전압과 퍼센트를 한 번의 ADC 읽기로 가져옵니다.
 *
 * @param status 상태를 저장할 구조체 포인터
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t battery_driver_update_status(battery_status_t* status);

/**
 * @brief 전압을 백분율로 변환
 * @param voltage 전압 (V)
 * @return 백분율 (0-100)
 */
uint8_t battery_driver_voltage_to_percent(float voltage);

/**
 * @brief 초기화 여부
 */
bool battery_driver_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_DRIVER_H
