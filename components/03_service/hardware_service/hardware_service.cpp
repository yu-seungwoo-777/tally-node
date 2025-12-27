/**
 * @file HardwareService.cpp
 * @brief 하드웨어 정보 수집 서비스 구현
 */

#include "hardware_service.h"
#include "t_log.h"
#include "event_bus.h"
#include "battery_driver.h"
#include "temperature_driver.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <cstring>

static const char* TAG = "HardwareSvc";

// ============================================================================
// HardwareService 클래스
// ============================================================================

class HardwareService {
public:
    static esp_err_t init(void);
    static void deinit(void);
    static bool isInitialized(void) { return s_initialized; }

    // Device ID
    static const char* getDeviceId(void);

    // Battery
    static void setBattery(uint8_t battery);
    static uint8_t updateBattery(void);
    static uint8_t getBattery(void);
    static float getVoltage(void);

    // Temperature
    static float getTemperature(void);

    // RSSI/SNR
    static void setRssi(int16_t rssi);
    static int16_t getRssi(void);
    static void setSnr(float snr);
    static float getSnr(void);

    // Uptime/Status
    static void setStopped(bool stopped);
    static bool getStopped(void);
    static void incUptime(void);
    static uint32_t getUptime(void);

    // System
    static void getSystem(hardware_system_t* status);

private:
    HardwareService() = delete;
    ~HardwareService() = delete;

    static void initDeviceId(void);
    static esp_err_t onRssiEvent(const event_data_t* event);

    static bool s_initialized;
    static bool s_device_id_initialized;
    static hardware_system_t s_system;
};

// ============================================================================
// 정적 변수
// ============================================================================

bool HardwareService::s_initialized = false;
bool HardwareService::s_device_id_initialized = false;
hardware_system_t HardwareService::s_system = {
    .device_id = "0000",
    .battery = 100,
    .voltage = 3.7f,
    .temperature = 25.0f,
    .rssi = -120,
    .snr = 0.0f,
    .uptime = 0,
    .stopped = false
};

// ============================================================================
// 내부 함수
// ============================================================================

void HardwareService::initDeviceId(void)
{
    if (s_device_id_initialized) {
        return;
    }

    // eFuse MAC 읽기 (WiFi STA MAC)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // MAC 뒤 4자리 사용 - 상위 nibble만 사용하여 4자리 hex 문자열 생성
    s_system.device_id[0] = "0123456789ABCDEF"[(mac[2] >> 4) & 0x0F];
    s_system.device_id[1] = "0123456789ABCDEF"[(mac[3] >> 4) & 0x0F];
    s_system.device_id[2] = "0123456789ABCDEF"[(mac[4] >> 4) & 0x0F];
    s_system.device_id[3] = "0123456789ABCDEF"[(mac[5] >> 4) & 0x0F];
    s_system.device_id[4] = '\0';

    s_device_id_initialized = true;
    T_LOGI(TAG, "Device ID: %s", s_system.device_id);
}

esp_err_t HardwareService::onRssiEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_rssi_event_t* status = (const lora_rssi_event_t*)event->data;
    s_system.rssi = status->rssi;
    s_system.snr = (float)status->snr;

    return ESP_OK;
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t HardwareService::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "HardwareService 초기화 중...");

    // Device ID 초기화
    initDeviceId();

    // 배터리 드라이버 초기화
    battery_driver_init();

    // 온도 센서 드라이버 초기화
    TemperatureDriver_init();

    // 배터리 읽기
    s_system.battery = updateBattery();

    // System 상태 기본값
    s_system.uptime = 0;
    s_system.stopped = false;
    s_system.rssi = -120;  // 기본값 (신호 없음)
    s_system.snr = 0.0f;   // 기본값

    // LoRa RSSI/SNR 이벤트 구독
    event_bus_subscribe(EVT_LORA_RSSI_CHANGED, onRssiEvent);

    s_initialized = true;
    T_LOGI(TAG, "HardwareService 초기화 완료");

    return ESP_OK;
}

void HardwareService::deinit(void)
{
    if (!s_initialized) {
        return;
    }

    // 이벤트 구독 취소
    event_bus_unsubscribe(EVT_LORA_RSSI_CHANGED, onRssiEvent);

    s_initialized = false;
    T_LOGI(TAG, "HardwareService 정리 완료");
}

// Device ID
const char* HardwareService::getDeviceId(void)
{
    if (!s_device_id_initialized) {
        initDeviceId();
    }
    return s_system.device_id;
}

// Battery
void HardwareService::setBattery(uint8_t battery)
{
    s_system.battery = battery;
}

uint8_t HardwareService::updateBattery(void)
{
    uint8_t percent = battery_driver_update_percent();
    s_system.battery = percent;
    return percent;
}

uint8_t HardwareService::getBattery(void)
{
    return s_system.battery;
}

float HardwareService::getVoltage(void)
{
    float voltage = 3.7f;  // 기본값
    battery_driver_get_voltage(&voltage);
    return voltage;
}

// Temperature
float HardwareService::getTemperature(void)
{
    float temp = 25.0f;  // 기본값
    TemperatureDriver_getCelsius(&temp);
    return temp;
}

// RSSI/SNR
void HardwareService::setRssi(int16_t rssi)
{
    s_system.rssi = rssi;
}

int16_t HardwareService::getRssi(void)
{
    return s_system.rssi;
}

void HardwareService::setSnr(float snr)
{
    s_system.snr = snr;
}

float HardwareService::getSnr(void)
{
    return s_system.snr;
}

// Uptime/Status
void HardwareService::setStopped(bool stopped)
{
    s_system.stopped = stopped;
}

bool HardwareService::getStopped(void)
{
    return s_system.stopped;
}

void HardwareService::incUptime(void)
{
    s_system.uptime++;
}

uint32_t HardwareService::getUptime(void)
{
    return s_system.uptime;
}

// System
void HardwareService::getSystem(hardware_system_t* status)
{
    if (status) {
        memcpy(status, &s_system, sizeof(hardware_system_t));
    }
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t hardware_service_init(void)
{
    return HardwareService::init();
}

esp_err_t hardware_service_deinit(void)
{
    HardwareService::deinit();
    return ESP_OK;
}

bool hardware_service_is_initialized(void)
{
    return HardwareService::isInitialized();
}

const char* hardware_service_get_device_id(void)
{
    return HardwareService::getDeviceId();
}

void hardware_service_set_battery(uint8_t battery)
{
    HardwareService::setBattery(battery);
}

uint8_t hardware_service_update_battery(void)
{
    return HardwareService::updateBattery();
}

uint8_t hardware_service_get_battery(void)
{
    return HardwareService::getBattery();
}

float hardware_service_get_voltage(void)
{
    return HardwareService::getVoltage();
}

float hardware_service_get_temperature(void)
{
    return HardwareService::getTemperature();
}

void hardware_service_set_rssi(int16_t rssi)
{
    HardwareService::setRssi(rssi);
}

int16_t hardware_service_get_rssi(void)
{
    return HardwareService::getRssi();
}

void hardware_service_set_snr(float snr)
{
    HardwareService::setSnr(snr);
}

float hardware_service_get_snr(void)
{
    return HardwareService::getSnr();
}

void hardware_service_set_stopped(bool stopped)
{
    HardwareService::setStopped(stopped);
}

bool hardware_service_get_stopped(void)
{
    return HardwareService::getStopped();
}

void hardware_service_inc_uptime(void)
{
    HardwareService::incUptime();
}

uint32_t hardware_service_get_uptime(void)
{
    return HardwareService::getUptime();
}

void hardware_service_get_system(hardware_system_t* status)
{
    HardwareService::getSystem(status);
}

}  // extern "C"
