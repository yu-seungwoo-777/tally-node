/**
 * @file temperature_hal.c
 * @brief 온도 센서 HAL 구현 (ESP32-S3 내장 온도 센서)
 *
 * ESP32-S3 내부 온도 센서를 사용하여 칩 온도를 측정합니다.
 * - 측정 범위: -10°C ~ 80°C
 * - 정확도: ±2°C (정격)
 */

#include "temperature_hal.h"
#include "t_log.h"
#include "esp_check.h"
#include "driver/temperature_sensor.h"

static const char* TAG = "05_Temp";

/**
 * @brief 온도 센서 측정 범위 (°C)
 *
 * ESP32-S3 내장 온도 센서의 정격 측정 범위
 */
#define TEMP_SENSOR_MIN_C  -10.0f  // 최저 측정 온도
#define TEMP_SENSOR_MAX_C   80.0f  // 최고 측정 온도

/**
 * @brief 섭씨를 화씨로 변환하기 위한 상수
 *
 * 변환 공식: F = C × 9/5 + 32
 */
#define CELSIUS_TO_FAHRENTEL_MULTIPLIER  9.0f
#define CELSIUS_TO_FAHRENTEL_DIVIDER    5.0f
#define FAHRENTEL_OFFSET               32.0f

// ============================================================================
// 내부 상태 변수
// ============================================================================

/** 온도 센서 핸들 */
static temperature_sensor_handle_t s_temp_sensor = NULL;

// ============================================================================
// 공개 API 구현
// ============================================================================

/**
 * @brief 온도 센서 HAL 초기화
 *
 * ESP32-S3 내부 온도 센서를 초기화하고 활성화합니다.
 * 이미 초기화된 경우 무시합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_init(void)
{
    T_LOGD(TAG, "init");

    // 이미 초기화된 경우
    if (s_temp_sensor != NULL) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    // 온도 센서 구성 (측정 범위: -10°C ~ 80°C)
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(
        (int)TEMP_SENSOR_MIN_C,
        (int)TEMP_SENSOR_MAX_C
    );

    // 온도 센서 설치 (메모리 할당 및 초기화)
    esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &s_temp_sensor);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:install:0x%x", ret);
        return ret;
    }

    // 온도 센서 활성화 (측정 시작)
    ret = temperature_sensor_enable(s_temp_sensor);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:enable:0x%x", ret);
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
        return ret;
    }

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief 온도 센서 HAL 해제
 *
 * 온도 센서를 비활성화하고 메모리를 해제합니다.
 */
void temperature_hal_deinit(void)
{
    T_LOGD(TAG, "deinit");

    if (s_temp_sensor != NULL) {
        temperature_sensor_disable(s_temp_sensor);
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
    }
}

/**
 * @brief 온도 읽기 (섭씨 단위)
 *
 * 현재 칩 온도를 섭씨 단위로 측정합니다.
 *
 * @param temp_c 온도(°C)를 저장할 포인터 (NULL 불가)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_read_celsius(float* temp_c)
{
    T_LOGD(TAG, "read_c");

    // 파라미터 유효성 검사
    if (temp_c == NULL) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    // 초기화 상태 확인
    if (s_temp_sensor == NULL) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    // 온도 읽기 (ESP-IDF 5.0+ API)
    esp_err_t ret = temperature_sensor_get_celsius(s_temp_sensor, temp_c);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:read:0x%x", ret);
        return ret;
    }

    T_LOGD(TAG, "ok:%.1fC", *temp_c);
    return ESP_OK;
}

/**
 * @brief 온도 읽기 (화씨 단위)
 *
 * 현재 칩 온도를 화씨 단위로 측정합니다.
 *
 * @param temp_f 온도(°F)를 저장할 포인터 (NULL 불가)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t temperature_hal_read_fahrenheit(float* temp_f)
{
    T_LOGD(TAG, "read_f");

    // 파라미터 유효성 검사
    if (temp_f == NULL) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    // 섭씨로 읽은 후 화씨로 변환
    float temp_c;
    esp_err_t ret = temperature_hal_read_celsius(&temp_c);
    if (ret == ESP_OK) {
        // F = C × 9/5 + 32
        *temp_f = temp_c * CELSIUS_TO_FAHRENTEL_MULTIPLIER / CELSIUS_TO_FAHRENTEL_DIVIDER + FAHRENTEL_OFFSET;
        T_LOGD(TAG, "ok:%.1fF", *temp_f);
    }

    return ret;
}
