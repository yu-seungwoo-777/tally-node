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
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <cstring>

static const char* TAG = "HardwareSvc";

// ============================================================================
// 태스크 설정
// ============================================================================

#define MONITOR_STACK_SIZE   3072
#define MONITOR_PRIORITY     4
#define MONITOR_INTERVAL_MS  1000   // 1초

// ============================================================================
// HardwareService 클래스
// ============================================================================

class HardwareService {
public:
    static esp_err_t init(void);
    static void deinit(void);
    static bool isInitialized(void) { return s_initialized; }

    // 태스크 제어
    static esp_err_t start(void);
    static void stop(void);
    static bool isRunning(void) { return s_running; }

    // Device ID
    static const char* getDeviceId(void);

    // Battery
    static void setBattery(uint8_t battery);
    static uint8_t updateBattery(void);
    static uint8_t getBattery(void);
    static float getVoltage(void);

    // Temperature (캐시된 값 반환)
    static float getTemperature(void);
    static void updateTemperature(void);  // 내부용: 실제 센서 읽기

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
    static esp_err_t onStopEvent(const event_data_t* event);
    static void hw_monitor_task(void* arg);

    static bool s_initialized;
    static bool s_running;
    static bool s_device_id_initialized;
    static TaskHandle_t s_task_handle;

    // 내부 상태 변수 (구조체 분리)
    static char s_device_id[5];
    static uint8_t s_battery;
    static float s_voltage;
    static float s_temperature;
    static uint8_t s_lora_chip_type;
    static uint32_t s_uptime;
    static bool s_stopped;
};

// ============================================================================
// 정적 변수
// ============================================================================

bool HardwareService::s_initialized = false;
bool HardwareService::s_running = false;
bool HardwareService::s_device_id_initialized = false;
TaskHandle_t HardwareService::s_task_handle = nullptr;

// 내부 상태 변수 초기화
char HardwareService::s_device_id[5] = "0000";
uint8_t HardwareService::s_battery = 100;
float HardwareService::s_voltage = 3.7f;
float HardwareService::s_temperature = 25.0f;
uint8_t HardwareService::s_lora_chip_type = 0;  // Unknown
uint32_t HardwareService::s_uptime = 0;
bool HardwareService::s_stopped = false;

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

    // MAC[4] + MAC[5] 사용하여 4자리 hex 문자열 생성
    s_device_id[0] = "0123456789ABCDEF"[mac[4] >> 4];      // MAC[4] 상위 4비트
    s_device_id[1] = "0123456789ABCDEF"[mac[4] & 0x0F];     // MAC[4] 하위 4비트
    s_device_id[2] = "0123456789ABCDEF"[mac[5] >> 4];      // MAC[5] 상위 4비트
    s_device_id[3] = "0123456789ABCDEF"[mac[5] & 0x0F];     // MAC[5] 하위 4비트
    s_device_id[4] = '\0';

    s_device_id_initialized = true;
    T_LOGI(TAG, "Device ID: %s", s_device_id);
}

esp_err_t HardwareService::onRssiEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_rssi_event_t* status = (const lora_rssi_event_t*)event->data;
    s_lora_chip_type = status->chip_type;

    return ESP_OK;
}

esp_err_t HardwareService::onStopEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    bool stopped = *(const bool*)event->data;
    s_stopped = stopped;

    T_LOGI(TAG, "정지 상태 변경: %s", stopped ? "정지" : "동작");

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
    s_battery = updateBattery();

    // System 상태 기본값
    s_uptime = 0;
    s_stopped = false;
    s_lora_chip_type = 0;  // Unknown

    // LoRa RSSI/SNR 이벤트 구독 (chip_type만 사용)
    event_bus_subscribe(EVT_LORA_RSSI_CHANGED, onRssiEvent);

    // 정지 상태 변경 이벤트 구독
    event_bus_subscribe(EVT_STOP_CHANGED, onStopEvent);

    s_initialized = true;
    T_LOGI(TAG, "HardwareService 초기화 완료");

    return ESP_OK;
}

void HardwareService::deinit(void)
{
    if (!s_initialized) {
        return;
    }

    // 태스크 정지
    stop();

    // 이벤트 구독 취소
    event_bus_unsubscribe(EVT_LORA_RSSI_CHANGED, onRssiEvent);
    event_bus_unsubscribe(EVT_STOP_CHANGED, onStopEvent);

    s_initialized = false;
    T_LOGI(TAG, "HardwareService 정리 완료");
}

// ============================================================================
// 태스크 관련
// ============================================================================

void HardwareService::hw_monitor_task(void* arg)
{
    (void)arg;
    T_LOGI(TAG, "하드웨어 모니터링 태스크 시작 (1초 주기)");

    while (s_running) {
        // 배터리 업데이트 (ADC 읽기)
        updateBattery();

        // 온도 업데이트 (실제 센서 읽기, 1초마다 갱신)
        updateTemperature();

        // uptime 증가
        s_uptime++;

        // 하드웨어 정보 이벤트 발행 (stack 변수 사용 - 이벤트 버스가 복사)
        system_info_event_t info;
        memcpy(info.device_id, s_device_id, sizeof(s_device_id));
        info.battery = s_battery;
        info.voltage = s_voltage;
        info.temperature = s_temperature;
        info.lora_chip_type = s_lora_chip_type;
        info.uptime = s_uptime;
        info.stopped = s_stopped;

        event_bus_publish(EVT_INFO_UPDATED, &info, sizeof(info));

        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    }

    T_LOGI(TAG, "모니터링 태스크 종료");
    vTaskDelete(nullptr);
}

esp_err_t HardwareService::start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        T_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    s_running = true;

    BaseType_t ret = xTaskCreate(
        hw_monitor_task,
        "hw_monitor_task",
        MONITOR_STACK_SIZE,
        nullptr,
        MONITOR_PRIORITY,
        &s_task_handle
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "태스크 생성 실패");
        s_running = false;
        return ESP_FAIL;
    }

    T_LOGI(TAG, "모니터링 태스크 시작");
    return ESP_OK;
}

void HardwareService::stop(void)
{
    if (!s_running) {
        return;
    }

    s_running = false;

    // 태스크 종료 대기
    if (s_task_handle != nullptr) {
        int wait_count = 0;
        while (eTaskGetState(s_task_handle) != eDeleted && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
        }
        s_task_handle = nullptr;
    }

    T_LOGI(TAG, "모니터링 태스크 정지");
}

// Device ID
const char* HardwareService::getDeviceId(void)
{
    if (!s_device_id_initialized) {
        initDeviceId();
    }
    return s_device_id;
}

// Battery
void HardwareService::setBattery(uint8_t battery)
{
    s_battery = battery;
}

uint8_t HardwareService::updateBattery(void)
{
    uint8_t percent = battery_driver_update_percent();
    s_battery = percent;

    // 전압도 함께 업데이트
    float voltage = 3.7f;  // 기본값
    if (battery_driver_get_voltage(&voltage) == ESP_OK) {
        s_voltage = voltage;
    }

    return percent;
}

uint8_t HardwareService::getBattery(void)
{
    return s_battery;
}

float HardwareService::getVoltage(void)
{
    float voltage = 3.7f;  // 기본값
    battery_driver_get_voltage(&voltage);
    return voltage;
}

// Temperature
void HardwareService::updateTemperature(void)
{
    float temp = 25.0f;  // 기본값
    if (TemperatureDriver_getCelsius(&temp) == ESP_OK) {
        s_temperature = temp;
    }
}

float HardwareService::getTemperature(void)
{
    // 캐시된 값 반환 (태스크에서 1초마다 갱신됨)
    return s_temperature;
}

// RSSI/SNR - device RF에서 관리하므로 hardware_service에서 제거
// void HardwareService::setRssi(int16_t rssi)
// {
//     s_rssi = rssi;
// }
//
// int16_t HardwareService::getRssi(void)
// {
//     return s_rssi;
// }
//
// void HardwareService::setSnr(float snr)
// {
//     s_snr = snr;
// }
//
// float HardwareService::getSnr(void)
// {
//     return s_snr;
// }

// Uptime/Status
void HardwareService::setStopped(bool stopped)
{
    s_stopped = stopped;
}

bool HardwareService::getStopped(void)
{
    return s_stopped;
}

void HardwareService::incUptime(void)
{
    s_uptime++;
}

uint32_t HardwareService::getUptime(void)
{
    return s_uptime;
}

// System
void HardwareService::getSystem(hardware_system_t* status)
{
    if (status) {
        // 개별 변수들을 구조체로 조합
        memcpy(status->device_id, s_device_id, sizeof(s_device_id));
        status->battery = s_battery;
        status->voltage = s_voltage;
        status->temperature = s_temperature;
        status->lora_chip_type = s_lora_chip_type;
        status->uptime = s_uptime;
        status->stopped = s_stopped;
    }
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t hardware_service_start(void)
{
    return HardwareService::start();
}

esp_err_t hardware_service_stop(void)
{
    HardwareService::stop();
    return ESP_OK;
}

bool hardware_service_is_running(void)
{
    return HardwareService::isRunning();
}

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

// RSSI/SNR - device RF에서 관리하므로 제거
// void hardware_service_set_rssi(int16_t rssi)
// {
//     HardwareService::setRssi(rssi);
// }
//
// int16_t hardware_service_get_rssi(void)
// {
//     return HardwareService::getRssi();
// }
//
// void hardware_service_set_snr(float snr)
// {
//     HardwareService::setSnr(snr);
// }
//
// float hardware_service_get_snr(void)
// {
//     return HardwareService::getSnr();
// }

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
