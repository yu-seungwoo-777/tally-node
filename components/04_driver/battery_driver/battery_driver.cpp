/**
 * @file battery_driver.cpp
 * @brief 배터리 드라이버 구현
 *
 * 배터리 전압을 측정하고 백분율로 변환합니다.
 * - HAL 계층에서 ADC 원시값을 읽어옴
 * - 18650 리튬이온 배터리 기준 전압-백분율 변환
 * - 전압 범위: 4.0V (100%) ~ 3.0V (0%)
 */

#include "battery_driver.h"
#include "battery_hal.h"
#include "t_log.h"

static const char* TAG = "04_BatteryDrv";

// ============================================================================
// 배터리 전압 상수 (18650 리튬이온 기준)
// ============================================================================

/**
 * @brief 배터리 전압 임계값 (V)
 *
 * 18650 리튬이온 배터리 방전 곡선 기준
 * 전압 0.1V당 약 10% 변환
 */
#define BATTERY_VOLTAGE_FULL    4.0f   // 100%
#define BATTERY_VOLTAGE_HIGH    3.9f   // 90%
#define BATTERY_VOLTAGE_NOMINAL 3.5f   // 50%
#define BATTERY_VOLTAGE_LOW     3.0f   // 0%

/**
 * @brief 최소 측정 가능 전압 (V)
 *
 * 이하 전압에서는 오차가 커서 기본값 반환
 */
#define BATTERY_VOLTAGE_MIN_VALID 0.5f

// ============================================================================
// BatteryDriver 클래스
// ============================================================================

class BatteryDriver {
public:
    /**
     * @brief 배터리 드라이버 초기화
     *
     * @return ESP_OK 성공, 에러 코드 실패
     */
    static esp_err_t init(void);

    /**
     * @brief 배터리 전압 읽기
     *
     * @param voltage 전압을 저장할 포인터 (V)
     * @return ESP_OK 성공, 에러 코드 실패
     */
    static esp_err_t getVoltage(float* voltage);

    /**
     * @brief 배터리 백분율 조회 (캐시된 값)
     *
     * @return 백분율 (0~100%)
     */
    static uint8_t getPercent(void);

    /**
     * @brief 배터리 백분율 갱신
     *
     * 전압을 측정하고 백분율로 변환합니다.
     *
     * @return 백분율 (0~100%)
     */
    static uint8_t updatePercent(void);

    /**
     * @brief 전압을 백분율로 변환
     *
     * 18650 리튬이온 배터리 기준 변환 표
     * - 4.0V 이상: 100%
     * - 3.9V ~ 4.0V: 90% ~ 100%
     * - 3.0V ~ 3.9V: 선형 보간
     * - 3.0V 미만: 0%
     *
     * @param voltage 배터리 전압 (V)
     * @return 백분율 (0~100%)
     */
    static uint8_t voltageToPercent(float voltage);

    /**
     * @brief 초기화 여부 확인
     *
     * @return true 초기화됨, false 초기화 안됨
     */
    static bool isInitialized(void) { return s_initialized; }

private:
    BatteryDriver() = delete;
    ~BatteryDriver() = delete;

    /** 초기화 완료 여부 */
    static bool s_initialized;
};

bool BatteryDriver::s_initialized = false;

// ============================================================================
// 공개 API 구현
// ============================================================================

esp_err_t BatteryDriver::init(void)
{
    T_LOGD(TAG, "init");

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    esp_err_t ret = battery_hal_init();
    if (ret == ESP_OK) {
        s_initialized = true;
        T_LOGD(TAG, "ok");
    } else {
        T_LOGE(TAG, "fail:0x%x", ret);
    }
    return ret;
}

esp_err_t BatteryDriver::getVoltage(float* voltage)
{
    T_LOGD(TAG, "getVoltage");

    if (!s_initialized) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    if (voltage == NULL) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = battery_hal_read_voltage(voltage);
    if (ret == ESP_OK) {
        T_LOGD(TAG, "ok:%.2fV", *voltage);
    }
    return ret;
}

uint8_t BatteryDriver::getPercent(void)
{
    float voltage;
    if (getVoltage(&voltage) == ESP_OK && voltage >= BATTERY_VOLTAGE_MIN_VALID) {
        return voltageToPercent(voltage);
    }
    return 100;  // 기본값 (전압 측정 실패 시)
}

uint8_t BatteryDriver::updatePercent(void)
{
    float voltage;
    if (getVoltage(&voltage) == ESP_OK && voltage >= BATTERY_VOLTAGE_MIN_VALID) {
        uint8_t percent = voltageToPercent(voltage);
        T_LOGD(TAG, "ok:%d%%", percent);
        return percent;
    }
    return 100;  // 기본값
}

uint8_t BatteryDriver::voltageToPercent(float voltage)
{
    // 18650 배터리: 0.1V당 10% (4.0V = 100%, 3.0V = 0%)
    if (voltage >= BATTERY_VOLTAGE_FULL) {
        return 100;
    } else if (voltage >= BATTERY_VOLTAGE_HIGH) {
        // 3.9V ~ 4.0V: 90% ~ 100%
        return (uint8_t)(90.0f + (voltage - BATTERY_VOLTAGE_HIGH) / 0.1f * 10.0f);
    } else if (voltage >= 3.8f) {
        return (uint8_t)(80.0f + (voltage - 3.8f) / 0.1f * 10.0f);
    } else if (voltage >= 3.7f) {
        return (uint8_t)(70.0f + (voltage - 3.7f) / 0.1f * 10.0f);
    } else if (voltage >= 3.6f) {
        return (uint8_t)(60.0f + (voltage - 3.6f) / 0.1f * 10.0f);
    } else if (voltage >= BATTERY_VOLTAGE_NOMINAL) {
        return (uint8_t)(50.0f + (voltage - BATTERY_VOLTAGE_NOMINAL) / 0.1f * 10.0f);
    } else if (voltage >= 3.4f) {
        return (uint8_t)(40.0f + (voltage - 3.4f) / 0.1f * 10.0f);
    } else if (voltage >= 3.3f) {
        return (uint8_t)(30.0f + (voltage - 3.3f) / 0.1f * 10.0f);
    } else if (voltage >= BATTERY_VOLTAGE_LOW) {
        // 3.0V ~ 3.3V: 0% ~ 30%
        return (uint8_t)((voltage - BATTERY_VOLTAGE_LOW) / 0.3f * 30.0f);
    }
    return 0;
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t battery_driver_init(void)
{
    return BatteryDriver::init();
}

esp_err_t battery_driver_get_voltage(float* voltage)
{
    return BatteryDriver::getVoltage(voltage);
}

uint8_t battery_driver_get_percent(void)
{
    return BatteryDriver::getPercent();
}

uint8_t battery_driver_update_percent(void)
{
    return BatteryDriver::updatePercent();
}

uint8_t battery_driver_voltage_to_percent(float voltage)
{
    return BatteryDriver::voltageToPercent(voltage);
}

bool battery_driver_is_initialized(void)
{
    return BatteryDriver::isInitialized();
}

}  // extern "C"
