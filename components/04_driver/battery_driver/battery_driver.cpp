/**
 * @file battery_driver.cpp
 * @brief 배터리 드라이버 구현 (전압 → 백분율 변환)
 */

#include "battery_driver.h"
#include "battery_hal.h"
#include "t_log.h"

class BatteryDriver {
public:
    static esp_err_t init(void);
    static esp_err_t getVoltage(float* voltage);
    static uint8_t getPercent(void);
    static uint8_t updatePercent(void);
    static uint8_t voltageToPercent(float voltage);
    static bool isInitialized(void) { return s_initialized; }

private:
    BatteryDriver() = delete;
    ~BatteryDriver() = delete;

    static bool s_initialized;
};

bool BatteryDriver::s_initialized = false;

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t BatteryDriver::init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = battery_hal_init();
    if (ret == ESP_OK) {
        s_initialized = true;
        T_LOGI("04_BatteryDrv", "배터리 드라이버 초기화 완료");
    }
    return ret;
}

esp_err_t BatteryDriver::getVoltage(float* voltage)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return battery_hal_read_voltage(voltage);
}

uint8_t BatteryDriver::getPercent(void)
{
    float voltage;
    if (getVoltage(&voltage) == ESP_OK && voltage > 0.5f) {
        return voltageToPercent(voltage);
    }
    return 100;  // 기본값
}

uint8_t BatteryDriver::updatePercent(void)
{
    float voltage;
    if (getVoltage(&voltage) == ESP_OK && voltage > 0.5f) {
        uint8_t percent = voltageToPercent(voltage);
        // T_LOGD("04_BatteryDrv", "배터리: %.2fV → %d%%", voltage, percent);
        return percent;
    }
    return 100;  // 기본값
}

uint8_t BatteryDriver::voltageToPercent(float voltage)
{
    // 18650 배터리: 0.1V당 10% (4.0V = 100%, 3.0V = 0%)
    if (voltage >= 4.0f) {
        return 100;
    } else if (voltage >= 3.9f) {
        return (uint8_t)(90.0f + (voltage - 3.9f) / 0.1f * 10.0f);
    } else if (voltage >= 3.8f) {
        return (uint8_t)(80.0f + (voltage - 3.8f) / 0.1f * 10.0f);
    } else if (voltage >= 3.7f) {
        return (uint8_t)(70.0f + (voltage - 3.7f) / 0.1f * 10.0f);
    } else if (voltage >= 3.6f) {
        return (uint8_t)(60.0f + (voltage - 3.6f) / 0.1f * 10.0f);
    } else if (voltage >= 3.5f) {
        return (uint8_t)(50.0f + (voltage - 3.5f) / 0.1f * 10.0f);
    } else if (voltage >= 3.4f) {
        return (uint8_t)(40.0f + (voltage - 3.4f) / 0.1f * 10.0f);
    } else if (voltage >= 3.3f) {
        return (uint8_t)(30.0f + (voltage - 3.3f) / 0.1f * 10.0f);
    } else if (voltage >= 3.0f) {
        return (uint8_t)((voltage - 3.0f) / 0.3f * 30.0f);
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
