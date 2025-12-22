/**
 * @file SystemMonitor.cpp
 * @brief 시스템 하드웨어 모니터링 Core 구현
 */


#include "log.h"
#include "log_tags.h"
#include "SystemMonitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/temperature_sensor.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = TAG_MONITOR;

// ============================================================================
// ADC 전압 측정
// ============================================================================

#define VOLTAGE_ADC_UNIT        ADC_UNIT_1
#define VOLTAGE_ADC_CHANNEL     ADC_CHANNEL_0  // GPIO1
#define VOLTAGE_ADC_ATTEN       ADC_ATTEN_DB_12
#define VOLTAGE_ADC_BITWIDTH    ADC_BITWIDTH_12
#define VOLTAGE_DIVIDER_RATIO   2.0f

static adc_oneshot_unit_handle_t s_adc_handle = nullptr;
static adc_cali_handle_t s_adc_cali_handle = nullptr;
static bool s_adc_calibrated = false;

static esp_err_t initVoltageADC()
{
    if (s_adc_handle != nullptr) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = VOLTAGE_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        LOG_0(TAG, "ADC 유닛 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = VOLTAGE_ADC_ATTEN,
        .bitwidth = VOLTAGE_ADC_BITWIDTH,
    };

    ret = adc_oneshot_config_channel(s_adc_handle, VOLTAGE_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        LOG_0(TAG, "ADC 채널 설정 실패: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = nullptr;
        return ret;
    }

    adc_cali_curve_fitting_config_t cali_config = {};
    cali_config.unit_id = VOLTAGE_ADC_UNIT;
    cali_config.atten = VOLTAGE_ADC_ATTEN;
    cali_config.bitwidth = VOLTAGE_ADC_BITWIDTH;

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_adc_cali_handle);
    if (ret == ESP_OK) {
        s_adc_calibrated = true;
        LOG_1(TAG, "ADC 캘리브레이션 성공");
    } else {
        s_adc_calibrated = false;
        LOG_1(TAG, "ADC 캘리브레이션 실패, Raw 값 사용");
    }

    LOG_1(TAG, "전압 측정 ADC 초기화 완료");
    return ESP_OK;
}

static esp_err_t readVoltage(float* voltage)
{
    if (s_adc_handle == nullptr) {
        esp_err_t ret = initVoltageADC();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    esp_err_t ret;
    int adc_value_mv = 0;

    if (s_adc_calibrated) {
        ret = adc_oneshot_get_calibrated_result(s_adc_handle, s_adc_cali_handle,
                                                 VOLTAGE_ADC_CHANNEL, &adc_value_mv);
        if (ret != ESP_OK) {
            LOG_0(TAG, "ADC 읽기 실패: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        int adc_raw;
        ret = adc_oneshot_read(s_adc_handle, VOLTAGE_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            LOG_0(TAG, "ADC 읽기 실패: %s", esp_err_to_name(ret));
            return ret;
        }
        adc_value_mv = (adc_raw * 3300) / 4095;
    }

    *voltage = (adc_value_mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;
    return ESP_OK;
}

static uint8_t calculateBatteryPercentage(float voltage)
{
    if (voltage >= 4.2f) {
        return 100;
    } else if (voltage >= 3.7f) {
        return 75 + (uint8_t)((voltage - 3.7f) / 0.5f * 25.0f);
    } else if (voltage >= 3.5f) {
        return 50 + (uint8_t)((voltage - 3.5f) / 0.2f * 25.0f);
    } else if (voltage >= 3.3f) {
        return 25 + (uint8_t)((voltage - 3.3f) / 0.2f * 25.0f);
    } else if (voltage >= 3.0f) {
        return (uint8_t)((voltage - 3.0f) / 0.3f * 25.0f);
    }
    return 0;
}

// ============================================================================
// 온도 센서
// ============================================================================

#define TEMP_RANGE_MIN      -10
#define TEMP_RANGE_MAX      80

static temperature_sensor_handle_t s_temp_sensor = nullptr;

static esp_err_t initTemperatureSensor()
{
    if (s_temp_sensor != nullptr) {
        return ESP_OK;
    }

    // 명시적으로 구조체 초기화하여 모든 경고 방지
    temperature_sensor_config_t temp_config = {
        .range_min = TEMP_RANGE_MIN,
        .range_max = TEMP_RANGE_MAX,
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
        .flags = 0
    };

    esp_err_t ret = temperature_sensor_install(&temp_config, &s_temp_sensor);
    if (ret != ESP_OK) {
        LOG_0(TAG, "온도 센서 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = temperature_sensor_enable(s_temp_sensor);
    if (ret != ESP_OK) {
        LOG_0(TAG, "온도 센서 활성화 실패: %s", esp_err_to_name(ret));
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = nullptr;
        return ret;
    }

    LOG_1(TAG, "온도 센서 초기화 완료");
    return ESP_OK;
}

static esp_err_t readTemperature(float* celsius)
{
    if (s_temp_sensor == nullptr) {
        esp_err_t ret = initTemperatureSensor();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    esp_err_t ret = temperature_sensor_get_celsius(s_temp_sensor, celsius);
    if (ret != ESP_OK) {
        LOG_0(TAG, "온도 측정 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t SystemMonitor::init()
{
    // ADC 초기화 (선택적)
    initVoltageADC();

    // 온도 센서 초기화 (선택적)
    initTemperatureSensor();

    LOG_0(TAG, "SystemMonitor 초기화 완료");
    return ESP_OK;
}

SystemHealth SystemMonitor::getHealth()
{
    SystemHealth health = {};

    health.uptime_sec = getUptime();
    health.temperature_celsius = getTemperature();
    health.voltage = getVoltage();
    health.battery_percent = getBatteryPercent();

    return health;
}

uint64_t SystemMonitor::getUptime()
{
    return esp_timer_get_time() / 1000000;
}

float SystemMonitor::getTemperature()
{
    float temp = 0.0f;
    if (readTemperature(&temp) == ESP_OK) {
        return temp;
    }
    return 0.0f;
}

float SystemMonitor::getVoltage()
{
    float voltage = 0.0f;
    if (readVoltage(&voltage) == ESP_OK) {
        return voltage;
    }
    return 0.0f;
}

uint8_t SystemMonitor::getBatteryPercent()
{
    float voltage = getVoltage();
    if (voltage > 0.0f) {
        return calculateBatteryPercentage(voltage);
    }
    return 0;
}
