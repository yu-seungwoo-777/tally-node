/**
 * @file TemperatureDriver.cpp
 * @brief 온도 센서 드라이버 구현 (C++)
 */

#include "temperature_driver.h"
#include "temperature_hal.h"
#include "t_log.h"

static const char* TAG = "04_Temp";
static bool s_initialized = false;

esp_err_t TemperatureDriver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = temperature_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "HAL 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    T_LOGI(TAG, "Temperature driver initialized");
    return ESP_OK;
}

void TemperatureDriver_deinit(void)
{
    if (s_initialized) {
        temperature_hal_deinit();
        s_initialized = false;
        T_LOGI(TAG, "Temperature driver deinitialized");
    }
}

esp_err_t TemperatureDriver_getCelsius(float* temp_c)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return temperature_hal_read_celsius(temp_c);
}

esp_err_t TemperatureDriver_getFahrenheit(float* temp_f)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return temperature_hal_read_fahrenheit(temp_f);
}
