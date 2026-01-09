/**
 * @file temperature_hal.c
 * @brief 온도 센서 HAL 구현 (ESP32-S3 내장 온도 센서)
 */

#include "temperature_hal.h"
#include "t_log.h"
#include "esp_check.h"
#include "driver/temperature_sensor.h"

static const char* TAG = "05_Temp";

// 온도 센서 핸들
static temperature_sensor_handle_t s_temp_sensor = NULL;

esp_err_t temperature_hal_init(void)
{
    if (s_temp_sensor != NULL) {
        return ESP_OK;  // 이미 초기화됨
    }

    // 온도 센서 구성
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    // 온도 센서 설치
    esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &s_temp_sensor);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "온도 센서 설치 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 온도 센서 시작
    ret = temperature_sensor_enable(s_temp_sensor);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "온도 센서 시작 실패: %s", esp_err_to_name(ret));
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
        return ret;
    }

    T_LOGI(TAG, "온도 센서 초기화 완료");
    return ESP_OK;
}

void temperature_hal_deinit(void)
{
    if (s_temp_sensor != NULL) {
        temperature_sensor_disable(s_temp_sensor);
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = NULL;
        T_LOGI(TAG, "온도 센서 해제됨");
    }
}

esp_err_t temperature_hal_read_celsius(float* temp_c)
{
    if (s_temp_sensor == NULL) {
        T_LOGE(TAG, "온도 센서가 초기화되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    if (temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 온도 읽기 (ESP-IDF 5.0+ API)
    esp_err_t ret = temperature_sensor_get_celsius(s_temp_sensor, temp_c);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "온도 읽기 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t temperature_hal_read_fahrenheit(float* temp_f)
{
    if (temp_f == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    float temp_c;
    esp_err_t ret = temperature_hal_read_celsius(&temp_c);
    if (ret == ESP_OK) {
        *temp_f = temp_c * 9.0f / 5.0f + 32.0f;
    }

    return ret;
}
