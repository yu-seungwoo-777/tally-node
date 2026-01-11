/**
 * @file battery_hal.c
 * @brief 배터리 전압 측정 HAL 구현
 *
 * ESP32-S3 내부 ADC를 사용하여 배터리 전압을 측정합니다.
 * - ADC1 채널 0 (GPIO1) 사용
 * - 12비트 해상도 (0~4095)
 * - 12dB 감쇠 (0~3300mV 입력 범위)
 * - 전압 분배비 2:1 고려
 */

#include "battery_hal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "t_log.h"
#include <stdbool.h>

static const char* TAG = "05_Battery";

// ============================================================================
// ADC 설정 상수
// ============================================================================

#define BATTERY_ADC_UNIT        ADC_UNIT_1     // ADC1 사용
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_0  // GPIO1 (배터리 전압 입력)
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12 // 12dB 감쇠 (입력 범위: 0~3300mV)
#define BATTERY_ADC_BITWIDTH    ADC_BITWIDTH_12 // 12비트 해상도 (0~4095)

/**
 * @brief 전압 분배비
 *
 * 외부 저항 분배 회로에 따른 비율
 * 예: 100kΩ/100kΩ 분배기 → 2.0f
 */
#define BATTERY_DIVIDER_RATIO   2.0f

/**
 * @brief ADC 원시값을 전압(mV)으로 변환하기 위한 상수
 *
 * ADC_ATTEN_DB_12 설정 시:
 * - 최대 입력 전압: 3300mV
 * - 최대 ADC 값: 4095 (12비트)
 * - 변환 공식: (raw * 3300) / 4095
 */
#define ADC_RAW_TO_MV_NUMERATOR   3300
#define ADC_RAW_TO_MV_DENOMINATOR 4095

// ============================================================================
// 내부 상태 변수
// ============================================================================

/** ADC oneshot 유닛 핸들 */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;

/** ADC 캘리브레이션 핸들 */
static adc_cali_handle_t s_adc_cali_handle = NULL;

/** 캘리브레이션 완료 여부 */
static bool s_adc_calibrated = false;

/** 초기화 완료 여부 */
static bool s_initialized = false;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief ADC 유닛 및 채널 초기화
 *
 * ADC1 유닛을 생성하고 채널 0을 설정합니다.
 * 캘리브레이션을 시도하고 실패 시 raw 값을 사용합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
static esp_err_t init_adc(void)
{
    T_LOGD(TAG, "init_adc");

    // 이미 초기화된 경우
    if (s_adc_handle != NULL) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    // ADC 유닛 설정
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    // ADC 유닛 생성
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:unit:0x%x", ret);
        return ret;
    }

    // 채널 설정
    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };

    ret = adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:ch:0x%x", ret);
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }

    // 캘리브레이션 시도
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_adc_cali_handle);
    if (ret == ESP_OK) {
        s_adc_calibrated = true;
        T_LOGD(TAG, "ok:cali");
    } else {
        s_adc_calibrated = false;
        T_LOGD(TAG, "ok:no_cali");
    }

    return ESP_OK;
}

// ============================================================================
// 공개 API 구현
// ============================================================================

/**
 * @brief 배터리 HAL 초기화
 *
 * ADC 유닛과 채널을 초기화합니다.
 * 이미 초기화된 경우 무시합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t battery_hal_init(void)
{
    T_LOGD(TAG, "init");

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    esp_err_t ret = init_adc();
    if (ret == ESP_OK) {
        s_initialized = true;
        T_LOGD(TAG, "ok");
    } else {
        T_LOGE(TAG, "fail:0x%x", ret);
    }

    return ret;
}

/**
 * @brief 배터리 전압 읽기 (볼트 단위)
 *
 * 현재 배터리 전압을 측정하여 반환합니다.
 *
 * @param voltage 전압을 저장할 포인터 (NULL 불가)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t battery_hal_read_voltage(float* voltage)
{
    T_LOGD(TAG, "read");

    // 파라미터 유효성 검사
    if (voltage == NULL) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    // 초기화 상태 확인
    if (!s_initialized) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_adc_handle == NULL) {
        T_LOGE(TAG, "fail:no_handle");
        return ESP_FAIL;
    }

    esp_err_t ret;
    int adc_value_mv = 0;

    // 캘리브레이션된 경우 보정 값 사용
    if (s_adc_calibrated) {
        ret = adc_oneshot_get_calibrated_result(s_adc_handle, s_adc_cali_handle,
                                                 BATTERY_ADC_CHANNEL, &adc_value_mv);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "fail:cali:0x%x", ret);
            return ret;
        }
    } else {
        // 캘리브레이션 실패 시 raw 값을 사용하여 직접 변환
        int adc_raw;
        ret = adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "fail:read:0x%x", ret);
            return ret;
        }
        // 원시값을 밀리볼트로 변환 (raw * 3300 / 4095)
        adc_value_mv = (adc_raw * ADC_RAW_TO_MV_NUMERATOR) / ADC_RAW_TO_MV_DENOMINATOR;
    }

    // 분배비를 고려하여 실제 배터리 전압 계산
    *voltage = (adc_value_mv / 1000.0f) * BATTERY_DIVIDER_RATIO;

    T_LOGD(TAG, "ok:%.2fV", *voltage);
    return ESP_OK;
}

/**
 * @brief 배터리 전압 읽기 (밀리볼트 단위)
 *
 * 현재 배터리 전압을 측정하여 밀리볼트 단위로 반환합니다.
 *
 * @param voltage_mv 전압(mV)을 저장할 포인터 (NULL 불가)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t battery_hal_read_voltage_mV(int* voltage_mv)
{
    T_LOGD(TAG, "read_mV");

    // 파라미터 유효성 검사
    if (voltage_mv == NULL) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    float voltage;
    esp_err_t ret = battery_hal_read_voltage(&voltage);
    if (ret == ESP_OK) {
        *voltage_mv = (int)(voltage * 1000.0f);
        T_LOGD(TAG, "ok:%dmV", *voltage_mv);
    }
    return ret;
}

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool battery_hal_is_initialized(void)
{
    return s_initialized;
}
