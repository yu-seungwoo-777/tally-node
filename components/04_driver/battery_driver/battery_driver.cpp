/**
 * @file battery_driver.cpp
 * @brief 배터리 드라이버 구현
 *
 * 배터리 전압을 측정하고 백분율로 변환합니다.
 * - HAL 계층에서 ADC 원시값을 읽어옴
 * - 18650 리튬이온 배터리 기준 비선형 방전 곡선 보정
 *
 * @section battery_curve 배터리 방전 곡선 (18650 리튬이온)
 *
 * | 전압   | 퍼센트 | 구간   |
 * |--------|--------|-------|
 * | ≥ 4.1V | 100%   | -     |
 * | 4.0V   | 80%    | 20%   |
 * | 3.9V   | 65%    | 15%   |
 * | 3.8V   | 50%    | 15%   |
 * | 3.7V   | 35%    | 15%   |
 * | 3.6V   | 20%    | 15%   |
 * | 3.5V   | 10%    | 10%   |
 * | 3.4V   | 5%     | 5%    |
 * | 3.3V   | 2%     | 3%    |
 * | 3.2V   | 0%     | 2%    |
 * | < 3.2V | 0%     | -     |
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
 * 18650 리튬이온 배터리 비선형 방전 곡선 기준
 */
#define BATTERY_VOLTAGE_FULL    4.1f   // 100% (완전충전)
#define BATTERY_VOLTAGE_LOW     3.2f   // 0%  (방전완료)

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
     * @brief 배터리 상태 업데이트 (전압 + 백분율, ADC 1회만 읽기)
     *
     * 중복 ADC 읽기를 방지하기 위해 전압과 퍼센트를 한 번의 ADC 읽기로 가져옵니다.
     *
     * @param status 상태를 저장할 구조체 포인터
     * @return ESP_OK 성공, 에러 코드 실패
     */
    static esp_err_t updateStatus(battery_status_t* status);

    /**
     * @brief 전압을 백분율로 변환
     *
     * 18650 리튬이온 배터리 기준 변환 표
     * - 4.0V 이상: 100%
     * - 3.9V ~ 4.0V: 90% ~ 100%
     * - 3.2V ~ 3.9V: 선형 보간
     * - 3.2V 미만: 0%
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

esp_err_t BatteryDriver::updateStatus(battery_status_t* status)
{
    if (status == nullptr) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        T_LOGE(TAG, "fail:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    // 전압 읽기 (ADC 1회)
    float voltage;
    esp_err_t ret = getVoltage(&voltage);
    if (ret != ESP_OK) {
        return ret;
    }

    // 전압과 퍼센트 한 번에 설정
    status->voltage = voltage;
    if (voltage >= BATTERY_VOLTAGE_MIN_VALID) {
        status->percent = voltageToPercent(voltage);
    } else {
        status->percent = 100;  // 기본값 (전압 측정 실패 시)
    }

    T_LOGD(TAG, "ok:%.2fV %d%%", status->voltage, status->percent);
    return ESP_OK;
}

uint8_t BatteryDriver::voltageToPercent(float voltage)
{
    // 18650 리튬이온 비선형 방전 곡선 보정
    // 방전 곡선: 4.2V(100%) ~ 3.2V(0%)

    if (voltage >= BATTERY_VOLTAGE_FULL) {
        return 100;
    } else if (voltage >= 4.1f) {
        // 4.1V ~ 4.2V: 90% ~ 100%
        return (uint8_t)(90.0f + (voltage - 4.1f) / 0.1f * 10.0f);
    } else if (voltage >= 4.0f) {
        // 4.0V ~ 4.1V: 80% ~ 90%
        return (uint8_t)(80.0f + (voltage - 4.0f) / 0.1f * 10.0f);
    } else if (voltage >= 3.9f) {
        // 3.9V ~ 4.0V: 65% ~ 80% (15% 구간)
        return (uint8_t)(65.0f + (voltage - 3.9f) / 0.1f * 15.0f);
    } else if (voltage >= 3.8f) {
        // 3.8V ~ 3.9V: 50% ~ 65% (15% 구간)
        return (uint8_t)(50.0f + (voltage - 3.8f) / 0.1f * 15.0f);
    } else if (voltage >= 3.7f) {
        // 3.7V ~ 3.8V: 35% ~ 50% (15% 구간)
        return (uint8_t)(35.0f + (voltage - 3.7f) / 0.1f * 15.0f);
    } else if (voltage >= 3.6f) {
        // 3.6V ~ 3.7V: 20% ~ 35% (15% 구간)
        return (uint8_t)(20.0f + (voltage - 3.6f) / 0.1f * 15.0f);
    } else if (voltage >= 3.5f) {
        // 3.5V ~ 3.6V: 10% ~ 20% (10% 구간)
        return (uint8_t)(10.0f + (voltage - 3.5f) / 0.1f * 10.0f);
    } else if (voltage >= 3.4f) {
        // 3.4V ~ 3.5V: 5% ~ 10% (5% 구간)
        return (uint8_t)(5.0f + (voltage - 3.4f) / 0.1f * 5.0f);
    } else if (voltage >= 3.3f) {
        // 3.3V ~ 3.4V: 2% ~ 5% (3% 구간)
        return (uint8_t)(2.0f + (voltage - 3.3f) / 0.1f * 3.0f);
    } else if (voltage >= BATTERY_VOLTAGE_LOW) {
        // 3.2V ~ 3.3V: 0% ~ 2% (2% 구간)
        return (uint8_t)((voltage - BATTERY_VOLTAGE_LOW) / 0.1f * 2.0f);
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

esp_err_t battery_driver_update_status(battery_status_t* status)
{
    return BatteryDriver::updateStatus(status);
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
