/**
 * @file battery_hal.c
 * @brief 배터리 전압 측정 HAL 구현
 */

#include "battery_hal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <stdbool.h>

static const char* TAG = "BatteryHal";

// ============================================================================
// ADC 설정
// ============================================================================

#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_0  // GPIO1
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12
#define BATTERY_ADC_BITWIDTH    ADC_BITWIDTH_12
#define BATTERY_DIVIDER_RATIO   2.0f

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;
static bool s_adc_calibrated = false;
static bool s_initialized = false;

// ============================================================================
// ADC 초기화
// ============================================================================

static esp_err_t init_adc(void)
{
    if (s_adc_handle != NULL) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC 유닛 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };

    ret = adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC 채널 설정 실패: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }

    adc_cali_curve_fitting_config_t cali_config = {};
    cali_config.unit_id = BATTERY_ADC_UNIT;
    cali_config.atten = BATTERY_ADC_ATTEN;
    cali_config.bitwidth = BATTERY_ADC_BITWIDTH;

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_adc_cali_handle);
    if (ret == ESP_OK) {
        s_adc_calibrated = true;
        ESP_LOGI(TAG, "ADC 캘리브레이션 성공");
    } else {
        s_adc_calibrated = false;
        ESP_LOGW(TAG, "ADC 캘리브레이션 실패, Raw 값 사용");
    }

    ESP_LOGI(TAG, "배터리 ADC 초기화 완료");
    return ESP_OK;
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t battery_hal_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = init_adc();
    if (ret == ESP_OK) {
        s_initialized = true;
    }
    return ret;
}

esp_err_t battery_hal_read_voltage(float* voltage)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_adc_handle == NULL) {
        return ESP_FAIL;
    }

    esp_err_t ret;
    int adc_value_mv = 0;

    if (s_adc_calibrated) {
        ret = adc_oneshot_get_calibrated_result(s_adc_handle, s_adc_cali_handle,
                                                 BATTERY_ADC_CHANNEL, &adc_value_mv);
        if (ret != ESP_OK) {
            return ret;
        }
    } else {
        int adc_raw;
        ret = adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            return ret;
        }
        adc_value_mv = (adc_raw * 3300) / 4095;
    }

    *voltage = (adc_value_mv / 1000.0f) * BATTERY_DIVIDER_RATIO;
    return ESP_OK;
}

esp_err_t battery_hal_read_voltage_mV(int* voltage_mv)
{
    float voltage;
    esp_err_t ret = battery_hal_read_voltage(&voltage);
    if (ret == ESP_OK) {
        *voltage_mv = (int)(voltage * 1000.0f);
    }
    return ret;
}

bool battery_hal_is_initialized(void)
{
    return s_initialized;
}
