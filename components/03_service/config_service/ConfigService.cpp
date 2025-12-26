/**
 * @file ConfigService.cpp
 * @brief NVS 설정 관리 서비스 구현 (C++)
 */

#include "ConfigService.h"
#include "NetworkConfig.h"
#include "LoRaConfig.h"
#include "battery_driver.h"
#include "TemperatureDriver.h"
#include "event_bus.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <cstring>

static const char* TAG = "ConfigService";

// ============================================================================
// System 상태 (RAM, 전역 변수)
// ============================================================================

static config_system_t s_system_state = {0};
static bool s_device_id_initialized = false;

// ============================================================================
// ConfigService 클래스 (싱글톤)
// ============================================================================

class ConfigServiceClass {
public:
    // 초기화
    static esp_err_t init(void);

    // 전체 설정 로드/저장
    static esp_err_t loadAll(config_all_t* config);
    static esp_err_t saveAll(const config_all_t* config);

    // WiFi AP 설정
    static esp_err_t getWiFiAP(config_wifi_ap_t* config);
    static esp_err_t setWiFiAP(const config_wifi_ap_t* config);

    // WiFi STA 설정
    static esp_err_t getWiFiSTA(config_wifi_sta_t* config);
    static esp_err_t setWiFiSTA(const config_wifi_sta_t* config);

    // Ethernet 설정
    static esp_err_t getEthernet(config_ethernet_t* config);
    static esp_err_t setEthernet(const config_ethernet_t* config);

    // Device 설정 (추가)
    static esp_err_t getDevice(config_device_t* config);
    static esp_err_t setDevice(const config_device_t* config);
    static esp_err_t setBrightness(uint8_t brightness);
    static esp_err_t setCameraId(uint8_t camera_id);
    static uint8_t getCameraId(void);
    static esp_err_t setRf(float frequency, uint8_t sync_word);

    // System 상태 (추가)
    static const char* getDeviceId(void);
    static void getSystem(config_system_t* status);
    static void setBattery(uint8_t battery);
    static uint8_t updateBattery(void);  // battery_driver 호출
    static void setStopped(bool stopped);
    static void incUptime(void);
    static void initDeviceId(void);

    // 기본값
    static esp_err_t loadDefaults(config_all_t* config);
    static esp_err_t factoryReset(void);

    // 유틸리티
    static bool isInitialized(void) { return s_initialized; }

private:
    ConfigServiceClass() = delete;
    ~ConfigServiceClass() = delete;

    // 정적 멤버
    static bool s_initialized;
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool ConfigServiceClass::s_initialized = false;

// ============================================================================
// 초기화
// ============================================================================

esp_err_t ConfigServiceClass::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "Config Service 초기화 중...");

    // Device ID 초기화 (eFuse MAC 읽기)
    initDeviceId();

    // 배터리 드라이버 초기화
    battery_driver_init();

    // 온도 센서 드라이버 초기화
    TemperatureDriver_init();

    // 배터리 읽기
    s_system_state.battery = updateBattery();

    // System 상태 기본값
    s_system_state.uptime = 0;
    s_system_state.stopped = false;

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_initialized = true;

    T_LOGI(TAG, "Config Service 초기화 완료");
    return ESP_OK;
}

// ============================================================================
// 전체 설정 로드/저장
// ============================================================================

esp_err_t ConfigServiceClass::loadAll(config_all_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_all_t));

    esp_err_t ret = getWiFiAP(&config->wifi_ap);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "WiFi AP 설정 로드 실패, 기본값 사용");
        loadDefaults(config);
        return ret;
    }

    getWiFiSTA(&config->wifi_sta);
    getEthernet(&config->ethernet);
    getDevice(&config->device);  // Device 설정 로드 추가

    return ESP_OK;
}

esp_err_t ConfigServiceClass::saveAll(const config_all_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    ret = setWiFiAP(&config->wifi_ap);
    if (ret != ESP_OK) return ret;

    ret = setWiFiSTA(&config->wifi_sta);
    if (ret != ESP_OK) return ret;

    ret = setEthernet(&config->ethernet);
    if (ret != ESP_OK) return ret;

    ret = setDevice(&config->device);  // Device 설정 저장 추가
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}

// ============================================================================
// WiFi AP 설정
// ============================================================================

esp_err_t ConfigServiceClass::getWiFiAP(config_wifi_ap_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_wifi_ap_t));

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = sizeof(config->ssid);
    nvs_get_str(handle, "wifi_ap_ssid", config->ssid, &len);

    len = sizeof(config->password);
    nvs_get_str(handle, "wifi_ap_pass", config->password, &len);

    uint8_t channel = 1;
    nvs_get_u8(handle, "wifi_ap_chan", &channel);
    config->channel = channel;

    uint8_t enabled = 1;
    nvs_get_u8(handle, "wifi_ap_enbl", &enabled);
    config->enabled = (enabled != 0);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setWiFiAP(const config_wifi_ap_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (config->ssid[0] != '\0') {
        nvs_set_str(handle, "wifi_ap_ssid", config->ssid);
    }
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_ap_pass", config->password);
    }
    nvs_set_u8(handle, "wifi_ap_chan", config->channel);
    nvs_set_u8(handle, "wifi_ap_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

// ============================================================================
// WiFi STA 설정
// ============================================================================

esp_err_t ConfigServiceClass::getWiFiSTA(config_wifi_sta_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_wifi_sta_t));

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = sizeof(config->ssid);
    nvs_get_str(handle, "wifi_sta_ssid", config->ssid, &len);

    len = sizeof(config->password);
    nvs_get_str(handle, "wifi_sta_pass", config->password, &len);

    uint8_t enabled = 0;
    nvs_get_u8(handle, "wifi_sta_enbl", &enabled);
    config->enabled = (enabled != 0);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setWiFiSTA(const config_wifi_sta_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (config->ssid[0] != '\0') {
        nvs_set_str(handle, "wifi_sta_ssid", config->ssid);
    }
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_sta_pass", config->password);
    }
    nvs_set_u8(handle, "wifi_sta_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

// ============================================================================
// Ethernet 설정
// ============================================================================

esp_err_t ConfigServiceClass::getEthernet(config_ethernet_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_ethernet_t));

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t dhcp_enabled = 1;
    nvs_get_u8(handle, "eth_dhcp_enbl", &dhcp_enabled);
    config->dhcp_enabled = (dhcp_enabled != 0);

    size_t len = sizeof(config->static_ip);
    nvs_get_str(handle, "eth_static_ip", config->static_ip, &len);

    len = sizeof(config->static_netmask);
    nvs_get_str(handle, "eth_static_net", config->static_netmask, &len);

    len = sizeof(config->static_gateway);
    nvs_get_str(handle, "eth_static_gw", config->static_gateway, &len);

    uint8_t enabled = 1;
    nvs_get_u8(handle, "eth_enbl", &enabled);
    config->enabled = (enabled != 0);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setEthernet(const config_ethernet_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "eth_dhcp_enbl", config->dhcp_enabled ? 1 : 0);

    if (config->static_ip[0] != '\0') {
        nvs_set_str(handle, "eth_static_ip", config->static_ip);
    }
    if (config->static_netmask[0] != '\0') {
        nvs_set_str(handle, "eth_static_net", config->static_netmask);
    }
    if (config->static_gateway[0] != '\0') {
        nvs_set_str(handle, "eth_static_gw", config->static_gateway);
    }

    nvs_set_u8(handle, "eth_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

// ============================================================================
// 기본값
// ============================================================================

esp_err_t ConfigServiceClass::loadDefaults(config_all_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_all_t));

    // WiFi AP 기본값 (NetworkConfig.h)
    strncpy(config->wifi_ap.ssid, WIFI_AP_SSID, sizeof(config->wifi_ap.ssid) - 1);
    strncpy(config->wifi_ap.password, WIFI_AP_PASSWORD, sizeof(config->wifi_ap.password) - 1);
    config->wifi_ap.channel = (WIFI_AP_CHANNEL > 0) ? WIFI_AP_CHANNEL : 1;
    config->wifi_ap.enabled = true;  // AP 활성화

    // WiFi STA 기본값 (NetworkConfig.h, AP+STA 모드)
    strncpy(config->wifi_sta.ssid, WIFI_STA_SSID, sizeof(config->wifi_sta.ssid) - 1);
    strncpy(config->wifi_sta.password, WIFI_STA_PASSWORD, sizeof(config->wifi_sta.password) - 1);
    config->wifi_sta.enabled = true;

    // Ethernet 기본값 (NetworkConfig.h)
    config->ethernet.dhcp_enabled = (DHCP_ENABLED != 0);
    strncpy(config->ethernet.static_ip, STATIC_IP, sizeof(config->ethernet.static_ip) - 1);
    strncpy(config->ethernet.static_netmask, STATIC_NETMASK, sizeof(config->ethernet.static_netmask) - 1);
    strncpy(config->ethernet.static_gateway, STATIC_GATEWAY, sizeof(config->ethernet.static_gateway) - 1);
    config->ethernet.enabled = true;

    // Device 기본값 (LoRaConfig.h)
    config->device.brightness = 128;      // 기본 밝기 50% (128/255)
    config->device.camera_id = 1;        // 기본 카메라 ID
    config->device.rf.frequency = LORA_DEFAULT_FREQ;
    config->device.rf.sync_word = LORA_DEFAULT_SYNC_WORD;

    T_LOGI(TAG, "기본값 로드됨");
    return ESP_OK;
}

esp_err_t ConfigServiceClass::factoryReset(void)
{
    T_LOGI(TAG, "공장 초기화 수행 중...");

    config_all_t defaultConfig;
    loadDefaults(&defaultConfig);

    return saveAll(&defaultConfig);
}

// ============================================================================
// Device 설정
// ============================================================================

esp_err_t ConfigServiceClass::getDevice(config_device_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_device_t));

    // 기본값 설정
    config->brightness = 128;  // 50%
    config->camera_id = 1;
    config->rf.frequency = LORA_DEFAULT_FREQ;
    config->rf.sync_word = LORA_DEFAULT_SYNC_WORD;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 열기 실패 시 기본값 반환
        return ESP_OK;
    }

    uint8_t brightness = 128;  // 기본값 50%
    if (nvs_get_u8(handle, "dev_brightness", &brightness) == ESP_OK) {
        config->brightness = brightness;
    }

    uint8_t camera_id = 1;
    if (nvs_get_u8(handle, "dev_camera_id", &camera_id) == ESP_OK) {
        config->camera_id = camera_id;
    }

    // RF 설정
    uint32_t freq_int = 0;
    if (nvs_get_u32(handle, "dev_frequency", &freq_int) == ESP_OK) {
        config->rf.frequency = freq_int / 10.0f;
    }

    uint8_t sync_word = LORA_DEFAULT_SYNC_WORD;
    if (nvs_get_u8(handle, "dev_sync_word", &sync_word) == ESP_OK) {
        config->rf.sync_word = sync_word;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setDevice(const config_device_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "dev_brightness", config->brightness);
    nvs_set_u8(handle, "dev_camera_id", config->camera_id);
    nvs_set_u32(handle, "dev_frequency", (uint32_t)(config->rf.frequency * 10));  // 소수점 저장 위해 *10
    nvs_set_u8(handle, "dev_sync_word", config->rf.sync_word);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setBrightness(uint8_t brightness)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev);
    // NVS에 값이 없어도 기본값으로 계속 진행
    if (ret != ESP_OK && ret != 0x105) {  // 0x105 = ESP_ERR_NVS_NOT_FOUND
        return ret;
    }

    dev.brightness = brightness;
    ret = setDevice(&dev);
    if (ret != ESP_OK) {
        return ret;
    }

    // 밝기 변경 이벤트 발행 (0-255 범위)
    event_bus_publish(EVT_BRIGHTNESS_CHANGED, &brightness, sizeof(brightness));
    T_LOGI(TAG, "밝기 변경: %d, 이벤트 발행", brightness);

    return ESP_OK;
}

esp_err_t ConfigServiceClass::setCameraId(uint8_t camera_id)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev);
    if (ret != ESP_OK && ret != 0x105) {
        return ret;
    }

    dev.camera_id = camera_id;
    return setDevice(&dev);
}

uint8_t ConfigServiceClass::getCameraId(void)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev);
    if (ret != ESP_OK) {
        return 1;  // 기본값
    }
    return dev.camera_id;
}

esp_err_t ConfigServiceClass::setRf(float frequency, uint8_t sync_word)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev);
    if (ret != ESP_OK && ret != 0x105) {
        return ret;
    }

    dev.rf.frequency = frequency;
    dev.rf.sync_word = sync_word;
    return setDevice(&dev);
}

// ============================================================================
// System 상태
// ============================================================================

void ConfigServiceClass::initDeviceId(void)
{
    if (s_device_id_initialized) {
        return;
    }

    // eFuse MAC 읽기 (WiFi STA MAC)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // MAC 뒤 4자리 사용 - 상위 nibble만 사용하여 4자리 hex 문자열 생성
    s_system_state.device_id[0] = "0123456789ABCDEF"[(mac[2] >> 4) & 0x0F];
    s_system_state.device_id[1] = "0123456789ABCDEF"[(mac[3] >> 4) & 0x0F];
    s_system_state.device_id[2] = "0123456789ABCDEF"[(mac[4] >> 4) & 0x0F];
    s_system_state.device_id[3] = "0123456789ABCDEF"[(mac[5] >> 4) & 0x0F];
    s_system_state.device_id[4] = '\0';

    s_device_id_initialized = true;

    T_LOGI(TAG, "Device ID: %s (from MAC)", s_system_state.device_id);
}

const char* ConfigServiceClass::getDeviceId(void)
{
    if (!s_device_id_initialized) {
        initDeviceId();
    }
    return s_system_state.device_id;
}

void ConfigServiceClass::getSystem(config_system_t* status)
{
    if (status) {
        memcpy(status, &s_system_state, sizeof(config_system_t));
    }
}

void ConfigServiceClass::setBattery(uint8_t battery)
{
    s_system_state.battery = battery;
}

void ConfigServiceClass::setStopped(bool stopped)
{
    s_system_state.stopped = stopped;
}

void ConfigServiceClass::incUptime(void)
{
    s_system_state.uptime++;
}

// ============================================================================
// 배터리
// ============================================================================

uint8_t ConfigServiceClass::updateBattery(void)
{
    uint8_t percent = battery_driver_update_percent();
    s_system_state.battery = percent;
    return percent;
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t config_service_init(void)
{
    return ConfigServiceClass::init();
}

esp_err_t config_service_load_all(config_all_t* config)
{
    return ConfigServiceClass::loadAll(config);
}

esp_err_t config_service_save_all(const config_all_t* config)
{
    return ConfigServiceClass::saveAll(config);
}

esp_err_t config_service_get_wifi_ap(config_wifi_ap_t* config)
{
    return ConfigServiceClass::getWiFiAP(config);
}

esp_err_t config_service_set_wifi_ap(const config_wifi_ap_t* config)
{
    return ConfigServiceClass::setWiFiAP(config);
}

esp_err_t config_service_get_wifi_sta(config_wifi_sta_t* config)
{
    return ConfigServiceClass::getWiFiSTA(config);
}

esp_err_t config_service_set_wifi_sta(const config_wifi_sta_t* config)
{
    return ConfigServiceClass::setWiFiSTA(config);
}

esp_err_t config_service_get_ethernet(config_ethernet_t* config)
{
    return ConfigServiceClass::getEthernet(config);
}

esp_err_t config_service_set_ethernet(const config_ethernet_t* config)
{
    return ConfigServiceClass::setEthernet(config);
}

esp_err_t config_service_load_defaults(config_all_t* config)
{
    return ConfigServiceClass::loadDefaults(config);
}

esp_err_t config_service_factory_reset(void)
{
    return ConfigServiceClass::factoryReset();
}

// ============================================================================
// Device 설정 API
// ============================================================================

esp_err_t config_service_get_device(config_device_t* config)
{
    return ConfigServiceClass::getDevice(config);
}

esp_err_t config_service_set_device(const config_device_t* config)
{
    return ConfigServiceClass::setDevice(config);
}

esp_err_t config_service_set_brightness(uint8_t brightness)
{
    return ConfigServiceClass::setBrightness(brightness);
}

esp_err_t config_service_set_camera_id(uint8_t camera_id)
{
    return ConfigServiceClass::setCameraId(camera_id);
}

uint8_t config_service_get_camera_id(void)
{
    return ConfigServiceClass::getCameraId();
}

uint8_t config_service_get_max_camera_num(void)
{
    // 기본 최대 카메라 번호 (추후 NVS에서 로드 가능)
    return 20;
}

esp_err_t config_service_set_rf(float frequency, uint8_t sync_word)
{
    return ConfigServiceClass::setRf(frequency, sync_word);
}

// ============================================================================
// System 상태 API
// ============================================================================

const char* config_service_get_device_id(void)
{
    return ConfigServiceClass::getDeviceId();
}

void config_service_get_system(config_system_t* status)
{
    ConfigServiceClass::getSystem(status);
}

void config_service_set_battery(uint8_t battery)
{
    ConfigServiceClass::setBattery(battery);
}

uint8_t config_service_update_battery(void)
{
    return ConfigServiceClass::updateBattery();
}

float config_service_get_voltage(void)
{
    float voltage = 3.7f;  // 기본값
    battery_driver_get_voltage(&voltage);
    return voltage;
}

float config_service_get_temperature(void)
{
    float temp = 25.0f;  // 기본값
    TemperatureDriver_getCelsius(&temp);
    return temp;
}

void config_service_set_stopped(bool stopped)
{
    ConfigServiceClass::setStopped(stopped);
}

void config_service_inc_uptime(void)
{
    ConfigServiceClass::incUptime();
}

// ============================================================================
// 기존 API
// ============================================================================

bool config_service_is_initialized(void)
{
    return ConfigServiceClass::isInitialized();
}

} // extern "C"
