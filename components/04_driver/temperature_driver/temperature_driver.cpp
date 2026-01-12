/**
 * @file TemperatureDriver.cpp
 * @brief 온도 센서 드라이버 구현 (C++)
 */

#include "temperature_driver.h"
#include "temperature_hal.h"
#include "t_log.h"

static const char* TAG = "04_Temp";
static bool s_initialized = false;

esp_err_t temperature_driver_init(void)
{
    T_LOGD(TAG, "init");

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    esp_err_t ret = temperature_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:0x%x", ret);
        return ret;
    }

    s_initialized = true;
    T_LOGD(TAG, "ok");
    return ESP_OK;
}

void temperature_driver_deinit(void)
{
    T_LOGD(TAG, "deinit");

    if (s_initialized) {
        temperature_hal_deinit();
        s_initialized = false;
    }
}

esp_err_t temperature_driver_get_celsius(float* temp_c)
{
    T_LOGD(TAG, "getc");

    if (!s_initialized) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    return temperature_hal_read_celsius(temp_c);
}

esp_err_t temperature_driver_get_fahrenheit(float* temp_f)
{
    T_LOGD(TAG, "getf");

    if (!s_initialized) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    return temperature_hal_read_fahrenheit(temp_f);
}
