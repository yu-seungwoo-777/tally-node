/**
 * @file ConfigService.cpp
 * @brief NVS 설정 관리 서비스 구현 (C++)
 */

#include "config_service.h"
#include "NVSConfig.h"
#include "event_bus.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "ConfigService";

// ============================================================================
// 내부 헬퍼 함수
// ============================================================================

// 4바이트 디바이스 ID 비교
static inline bool device_id_equals(const uint8_t* a, const uint8_t* b) {
    return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

// 디바이스 ID를 문자열로 변환 (디버깅용)
static void device_id_to_str(const uint8_t* id, char* out) {
    snprintf(out, 5, "%02X%02X", id[0], id[1]);
}

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

    // Device 설정
    static esp_err_t getDevice(config_device_t* config);
    static esp_err_t setDevice(const config_device_t* config);
    static esp_err_t setBrightness(uint8_t brightness);
    static esp_err_t setCameraId(uint8_t camera_id);
    static uint8_t getCameraId(void);
    static esp_err_t setRf(float frequency, uint8_t sync_word);

    // Switcher 설정
    static esp_err_t getPrimary(config_switcher_t* config);
    static esp_err_t setPrimary(const config_switcher_t* config);
    static esp_err_t getSecondary(config_switcher_t* config);
    static esp_err_t setSecondary(const config_switcher_t* config);
    static bool getDualEnabled(void);
    static esp_err_t setDualEnabled(bool enabled);
    static uint8_t getSecondaryOffset(void);
    static esp_err_t setSecondaryOffset(uint8_t offset);

    // LED 색상 설정
    static esp_err_t getLedColors(config_led_colors_t* config);
    static esp_err_t setLedColors(const config_led_colors_t* config);
    static void getLedProgramColor(uint8_t* r, uint8_t* g, uint8_t* b);
    static void getLedPreviewColor(uint8_t* r, uint8_t* g, uint8_t* b);
    static void getLedOffColor(uint8_t* r, uint8_t* g, uint8_t* b);
    static void getLedBatteryLowColor(uint8_t* r, uint8_t* g, uint8_t* b);

    // 등록된 디바이스 관리
    static esp_err_t registerDevice(const uint8_t* device_id);
    static esp_err_t unregisterDevice(const uint8_t* device_id);
    static bool isDeviceRegistered(const uint8_t* device_id);
    static esp_err_t getRegisteredDevices(config_registered_devices_t* devices);
    static uint8_t getRegisteredDeviceCount(void);
    static void clearRegisteredDevices(void);

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
// 이벤트 핸들러 (등록된 디바이스 관리)
// ============================================================================

/**
 * @brief 디바이스 등록 요청 이벤트 핸들러
 */
static esp_err_t on_device_register_request(const event_data_t* event) {
    if (event->type != EVT_DEVICE_REGISTER) {
        return ESP_OK;
    }

    const auto* req = reinterpret_cast<const device_register_event_t*>(event->data);
    if (req == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return ConfigServiceClass::registerDevice(req->device_id);
}

/**
 * @brief 디바이스 등록 해제 요청 이벤트 핸들러
 */
static esp_err_t on_device_unregister_request(const event_data_t* event) {
    if (event->type != EVT_DEVICE_UNREGISTER) {
        return ESP_OK;
    }

    const auto* req = reinterpret_cast<const device_register_event_t*>(event->data);
    if (req == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return ConfigServiceClass::unregisterDevice(req->device_id);
}

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

    // NVS 초기화 (이미 초기화된 경우 무시)
    T_LOGD(TAG, "NVS flash init 시작...");
    esp_err_t ret = nvs_flash_init();
    T_LOGD(TAG, "NVS flash init 결과: %d", ret);

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        T_LOGI(TAG, "NVS erase 필요");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            const char* err_name = esp_err_to_name(ret);
            T_LOGE(TAG, "NVS erase 실패: %s", err_name ? err_name : "unknown");
            return ret;
        }
        T_LOGD(TAG, "NVS re-init 시작...");
        ret = nvs_flash_init();
        T_LOGD(TAG, "NVS re-init 결과: %d", ret);
    }

    // 초기화 결과 확인
    if (ret == ESP_ERR_INVALID_STATE) {
        // 이미 초기화됨, 무시
        T_LOGD(TAG, "NVS 이미 초기화됨");
    } else if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    T_LOGD(TAG, "Event bus 구독 시작...");
    // 디바이스 등록/해제 이벤트 구독
    event_bus_subscribe(EVT_DEVICE_REGISTER, on_device_register_request);
    event_bus_subscribe(EVT_DEVICE_UNREGISTER, on_device_unregister_request);
    T_LOGD(TAG, "Event bus 구독 완료");

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
    getDevice(&config->device);
    getPrimary(&config->primary);
    getSecondary(&config->secondary);
    config->dual_enabled = getDualEnabled();
    config->secondary_offset = getSecondaryOffset();

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

    ret = setDevice(&config->device);
    if (ret != ESP_OK) return ret;

    ret = setPrimary(&config->primary);
    if (ret != ESP_OK) return ret;

    ret = setSecondary(&config->secondary);
    if (ret != ESP_OK) return ret;

    ret = setDualEnabled(config->dual_enabled);
    if (ret != ESP_OK) return ret;

    ret = setSecondaryOffset(config->secondary_offset);
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

    // WiFi AP 기본값
    strncpy(config->wifi_ap.ssid, NVS_WIFI_AP_SSID, sizeof(config->wifi_ap.ssid) - 1);
    strncpy(config->wifi_ap.password, NVS_WIFI_AP_PASSWORD, sizeof(config->wifi_ap.password) - 1);
    config->wifi_ap.channel = NVS_WIFI_AP_CHANNEL;
    config->wifi_ap.enabled = true;

    // WiFi STA 기본값
    strncpy(config->wifi_sta.ssid, NVS_WIFI_STA_SSID, sizeof(config->wifi_sta.ssid) - 1);
    strncpy(config->wifi_sta.password, NVS_WIFI_STA_PASSWORD, sizeof(config->wifi_sta.password) - 1);
    config->wifi_sta.enabled = true;

    // Ethernet 기본값
    config->ethernet.dhcp_enabled = (NVS_ETHERNET_DHCP_ENABLED != 0);
    strncpy(config->ethernet.static_ip, NVS_ETHERNET_STATIC_IP, sizeof(config->ethernet.static_ip) - 1);
    strncpy(config->ethernet.static_netmask, NVS_ETHERNET_STATIC_NETMASK, sizeof(config->ethernet.static_netmask) - 1);
    strncpy(config->ethernet.static_gateway, NVS_ETHERNET_STATIC_GATEWAY, sizeof(config->ethernet.static_gateway) - 1);
    config->ethernet.enabled = true;

    // Device 기본값
    config->device.brightness = NVS_DEVICE_BRIGHTNESS;
    config->device.camera_id = NVS_DEVICE_CAMERA_ID;
    config->device.rf.frequency = NVS_LORA_DEFAULT_FREQ;
    config->device.rf.sync_word = NVS_LORA_DEFAULT_SYNC_WORD;
    config->device.rf.sf = NVS_LORA_DEFAULT_SF;
    config->device.rf.cr = NVS_LORA_DEFAULT_CR;
    config->device.rf.bw = NVS_LORA_DEFAULT_BW;
    config->device.rf.tx_power = NVS_LORA_DEFAULT_TX_POWER;

    // Switcher 기본값 - Primary
    config->primary.type = NVS_SWITCHER_PRI_TYPE;
    strncpy(config->primary.ip, NVS_SWITCHER_PRI_IP, sizeof(config->primary.ip) - 1);
    config->primary.port = NVS_SWITCHER_PRI_PORT;
    strncpy(config->primary.password, NVS_SWITCHER_PRI_PASSWORD, sizeof(config->primary.password) - 1);
    config->primary.interface = NVS_SWITCHER_PRI_INTERFACE;
    config->primary.camera_limit = NVS_SWITCHER_PRI_CAMERA_LIMIT;

    // Switcher 기본값 - Secondary
    config->secondary.type = NVS_SWITCHER_SEC_TYPE;
    strncpy(config->secondary.ip, NVS_SWITCHER_SEC_IP, sizeof(config->secondary.ip) - 1);
    config->secondary.port = NVS_SWITCHER_SEC_PORT;
    strncpy(config->secondary.password, NVS_SWITCHER_SEC_PASSWORD, sizeof(config->secondary.password) - 1);
    config->secondary.interface = NVS_SWITCHER_SEC_INTERFACE;
    config->secondary.camera_limit = NVS_SWITCHER_SEC_CAMERA_LIMIT;

    // Dual 모드 설정
    config->dual_enabled = NVS_DUAL_ENABLED;
    config->secondary_offset = NVS_DUAL_OFFSET;

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

    // 기본값 설정 (NVSConfig)
    config->brightness = NVS_DEVICE_BRIGHTNESS;
    config->camera_id = NVS_DEVICE_CAMERA_ID;
    config->rf.frequency = NVS_LORA_DEFAULT_FREQ;
    config->rf.sync_word = NVS_LORA_DEFAULT_SYNC_WORD;
    config->rf.sf = NVS_LORA_DEFAULT_SF;
    config->rf.cr = NVS_LORA_DEFAULT_CR;
    config->rf.bw = NVS_LORA_DEFAULT_BW;
    config->rf.tx_power = NVS_LORA_DEFAULT_TX_POWER;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 열기 실패 시 기본값 반환
        return ESP_OK;
    }

    uint8_t brightness = NVS_DEVICE_BRIGHTNESS;
    if (nvs_get_u8(handle, "dev_brightness", &brightness) == ESP_OK) {
        config->brightness = brightness;
    }

    uint8_t camera_id = NVS_DEVICE_CAMERA_ID;
    if (nvs_get_u8(handle, "dev_camera_id", &camera_id) == ESP_OK) {
        config->camera_id = camera_id;
    }

    // RF 설정
    uint32_t freq_int = 0;
    if (nvs_get_u32(handle, "dev_frequency", &freq_int) == ESP_OK) {
        config->rf.frequency = freq_int / 10.0f;
    }

    uint8_t sync_word = NVS_LORA_DEFAULT_SYNC_WORD;
    if (nvs_get_u8(handle, "dev_sync_word", &sync_word) == ESP_OK) {
        config->rf.sync_word = sync_word;
    }

    uint8_t sf = NVS_LORA_DEFAULT_SF;
    if (nvs_get_u8(handle, "dev_sf", &sf) == ESP_OK) {
        config->rf.sf = sf;
    }

    uint8_t cr = NVS_LORA_DEFAULT_CR;
    if (nvs_get_u8(handle, "dev_cr", &cr) == ESP_OK) {
        config->rf.cr = cr;
    }

    uint32_t bw_int = (uint32_t)(NVS_LORA_DEFAULT_BW * 10);  // 250.0 * 10
    if (nvs_get_u32(handle, "dev_bw", &bw_int) == ESP_OK) {
        config->rf.bw = bw_int / 10.0f;
    }

    int8_t tx_power = NVS_LORA_DEFAULT_TX_POWER;
    if (nvs_get_i8(handle, "dev_tx_power", &tx_power) == ESP_OK) {
        config->rf.tx_power = tx_power;
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
    nvs_set_u8(handle, "dev_sf", config->rf.sf);
    nvs_set_u8(handle, "dev_cr", config->rf.cr);
    nvs_set_u32(handle, "dev_bw", (uint32_t)(config->rf.bw * 10));  // 소수점 저장 위해 *10
    nvs_set_i8(handle, "dev_tx_power", config->rf.tx_power);

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
// Switcher 설정
// ============================================================================

esp_err_t ConfigServiceClass::getPrimary(config_switcher_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_switcher_t));

    // 기본값 설정 (NVSConfig)
    config->type = NVS_SWITCHER_PRI_TYPE;
    config->port = NVS_SWITCHER_PRI_PORT;
    config->interface = NVS_SWITCHER_PRI_INTERFACE;
    config->camera_limit = NVS_SWITCHER_PRI_CAMERA_LIMIT;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 열기 실패 시 기본값 반환
        return ESP_OK;
    }

    uint8_t type = NVS_SWITCHER_PRI_TYPE;
    if (nvs_get_u8(handle, "sw_pri_type", &type) == ESP_OK) {
        config->type = type;
    }

    size_t len = sizeof(config->ip);
    nvs_get_str(handle, "sw_pri_ip", config->ip, &len);

    uint16_t port = NVS_SWITCHER_PRI_PORT;
    if (nvs_get_u16(handle, "sw_pri_port", &port) == ESP_OK) {
        config->port = port;
    }

    len = sizeof(config->password);
    nvs_get_str(handle, "sw_pri_pass", config->password, &len);

    uint8_t interface = NVS_SWITCHER_PRI_INTERFACE;
    if (nvs_get_u8(handle, "sw_pri_if", &interface) == ESP_OK) {
        config->interface = interface;
    }

    uint8_t camera_limit = NVS_SWITCHER_PRI_CAMERA_LIMIT;
    if (nvs_get_u8(handle, "sw_pri_limit", &camera_limit) == ESP_OK) {
        config->camera_limit = camera_limit;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setPrimary(const config_switcher_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "sw_pri_type", config->type);
    nvs_set_str(handle, "sw_pri_ip", config->ip);
    nvs_set_u16(handle, "sw_pri_port", config->port);
    nvs_set_str(handle, "sw_pri_pass", config->password);
    nvs_set_u8(handle, "sw_pri_if", config->interface);
    nvs_set_u8(handle, "sw_pri_limit", config->camera_limit);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::getSecondary(config_switcher_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_switcher_t));

    // 기본값 설정 (NVSConfig)
    config->type = NVS_SWITCHER_SEC_TYPE;
    config->port = NVS_SWITCHER_SEC_PORT;
    config->interface = NVS_SWITCHER_SEC_INTERFACE;
    config->camera_limit = NVS_SWITCHER_SEC_CAMERA_LIMIT;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 열기 실패 시 기본값 반환
        return ESP_OK;
    }

    uint8_t type = NVS_SWITCHER_SEC_TYPE;
    if (nvs_get_u8(handle, "sw_sec_type", &type) == ESP_OK) {
        config->type = type;
    }

    size_t len = sizeof(config->ip);
    nvs_get_str(handle, "sw_sec_ip", config->ip, &len);

    uint16_t port = NVS_SWITCHER_SEC_PORT;
    if (nvs_get_u16(handle, "sw_sec_port", &port) == ESP_OK) {
        config->port = port;
    }

    len = sizeof(config->password);
    nvs_get_str(handle, "sw_sec_pass", config->password, &len);

    uint8_t interface = NVS_SWITCHER_SEC_INTERFACE;
    if (nvs_get_u8(handle, "sw_sec_if", &interface) == ESP_OK) {
        config->interface = interface;
    }

    uint8_t camera_limit = NVS_SWITCHER_SEC_CAMERA_LIMIT;
    if (nvs_get_u8(handle, "sw_sec_limit", &camera_limit) == ESP_OK) {
        config->camera_limit = camera_limit;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setSecondary(const config_switcher_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "sw_sec_type", config->type);
    nvs_set_str(handle, "sw_sec_ip", config->ip);
    nvs_set_u16(handle, "sw_sec_port", config->port);
    nvs_set_str(handle, "sw_sec_pass", config->password);
    nvs_set_u8(handle, "sw_sec_if", config->interface);
    nvs_set_u8(handle, "sw_sec_limit", config->camera_limit);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

bool ConfigServiceClass::getDualEnabled(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return NVS_DUAL_ENABLED;  // 기본값
    }

    uint8_t enabled = 0;
    nvs_get_u8(handle, "sw_dual_enbl", &enabled);

    nvs_close(handle);
    return (enabled != 0);
}

esp_err_t ConfigServiceClass::setDualEnabled(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "sw_dual_enbl", enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

uint8_t ConfigServiceClass::getSecondaryOffset(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return NVS_DUAL_OFFSET;  // 기본값
    }

    uint8_t offset = NVS_DUAL_OFFSET;
    nvs_get_u8(handle, "sw_dual_offset", &offset);

    nvs_close(handle);
    return offset;
}

esp_err_t ConfigServiceClass::setSecondaryOffset(uint8_t offset)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u8(handle, "sw_dual_offset", offset);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

// ============================================================================
// LED 색상 설정
// ============================================================================

esp_err_t ConfigServiceClass::getLedColors(config_led_colors_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_led_colors_t));

    // 기본값 설정 (NVSConfig)
    config->program.r = NVS_LED_PROGRAM_R;
    config->program.g = NVS_LED_PROGRAM_G;
    config->program.b = NVS_LED_PROGRAM_B;

    config->preview.r = NVS_LED_PREVIEW_R;
    config->preview.g = NVS_LED_PREVIEW_G;
    config->preview.b = NVS_LED_PREVIEW_B;

    config->off.r = NVS_LED_OFF_R;
    config->off.g = NVS_LED_OFF_G;
    config->off.b = NVS_LED_OFF_B;

    config->battery_low.r = NVS_LED_BATTERY_LOW_R;
    config->battery_low.g = NVS_LED_BATTERY_LOW_G;
    config->battery_low.b = NVS_LED_BATTERY_LOW_B;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 열기 실패 시 기본값 반환
        return ESP_OK;
    }

    // PROGRAM 색상
    uint8_t r = NVS_LED_PROGRAM_R;
    nvs_get_u8(handle, "led_pgm_r", &r);
    config->program.r = r;
    uint8_t g = NVS_LED_PROGRAM_G;
    nvs_get_u8(handle, "led_pgm_g", &g);
    config->program.g = g;
    uint8_t b = NVS_LED_PROGRAM_B;
    nvs_get_u8(handle, "led_pgm_b", &b);
    config->program.b = b;

    // PREVIEW 색상
    r = NVS_LED_PREVIEW_R;
    nvs_get_u8(handle, "led_pvw_r", &r);
    config->preview.r = r;
    g = NVS_LED_PREVIEW_G;
    nvs_get_u8(handle, "led_pvw_g", &g);
    config->preview.g = g;
    b = NVS_LED_PREVIEW_B;
    nvs_get_u8(handle, "led_pvw_b", &b);
    config->preview.b = b;

    // OFF 색상
    r = NVS_LED_OFF_R;
    nvs_get_u8(handle, "led_off_r", &r);
    config->off.r = r;
    g = NVS_LED_OFF_G;
    nvs_get_u8(handle, "led_off_g", &g);
    config->off.g = g;
    b = NVS_LED_OFF_B;
    nvs_get_u8(handle, "led_off_b", &b);
    config->off.b = b;

    // BATTERY_LOW 색상
    r = NVS_LED_BATTERY_LOW_R;
    nvs_get_u8(handle, "led_bat_r", &r);
    config->battery_low.r = r;
    g = NVS_LED_BATTERY_LOW_G;
    nvs_get_u8(handle, "led_bat_g", &g);
    config->battery_low.g = g;
    b = NVS_LED_BATTERY_LOW_B;
    nvs_get_u8(handle, "led_bat_b", &b);
    config->battery_low.b = b;

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigServiceClass::setLedColors(const config_led_colors_t* config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("config", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // PROGRAM 색상
    nvs_set_u8(handle, "led_pgm_r", config->program.r);
    nvs_set_u8(handle, "led_pgm_g", config->program.g);
    nvs_set_u8(handle, "led_pgm_b", config->program.b);

    // PREVIEW 색상
    nvs_set_u8(handle, "led_pvw_r", config->preview.r);
    nvs_set_u8(handle, "led_pvw_g", config->preview.g);
    nvs_set_u8(handle, "led_pvw_b", config->preview.b);

    // OFF 색상
    nvs_set_u8(handle, "led_off_r", config->off.r);
    nvs_set_u8(handle, "led_off_g", config->off.g);
    nvs_set_u8(handle, "led_off_b", config->off.b);

    // BATTERY_LOW 색상
    nvs_set_u8(handle, "led_bat_r", config->battery_low.r);
    nvs_set_u8(handle, "led_bat_g", config->battery_low.g);
    nvs_set_u8(handle, "led_bat_b", config->battery_low.b);

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

void ConfigServiceClass::getLedProgramColor(uint8_t* r, uint8_t* g, uint8_t* b)
{
    config_led_colors_t colors;
    getLedColors(&colors);
    if (r) *r = colors.program.r;
    if (g) *g = colors.program.g;
    if (b) *b = colors.program.b;
}

void ConfigServiceClass::getLedPreviewColor(uint8_t* r, uint8_t* g, uint8_t* b)
{
    config_led_colors_t colors;
    getLedColors(&colors);
    if (r) *r = colors.preview.r;
    if (g) *g = colors.preview.g;
    if (b) *b = colors.preview.b;
}

void ConfigServiceClass::getLedOffColor(uint8_t* r, uint8_t* g, uint8_t* b)
{
    config_led_colors_t colors;
    getLedColors(&colors);
    if (r) *r = colors.off.r;
    if (g) *g = colors.off.g;
    if (b) *b = colors.off.b;
}

void ConfigServiceClass::getLedBatteryLowColor(uint8_t* r, uint8_t* g, uint8_t* b)
{
    config_led_colors_t colors;
    getLedColors(&colors);
    if (r) *r = colors.battery_low.r;
    if (g) *g = colors.battery_low.g;
    if (b) *b = colors.battery_low.b;
}

// ============================================================================
// 등록된 디바이스 관리
// ============================================================================

// NVS 네임스페이스 및 키
#define NVS_NAMESPACE_DEVICES "dev_mgmt"
#define NVS_KEY_DEVICE_COUNT "reg_count"
#define NVS_KEY_DEVICE_PREFIX "dev_"

esp_err_t ConfigServiceClass::registerDevice(const uint8_t* device_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

    // 이미 등록되어 있는지 확인
    if (isDeviceRegistered(device_id)) {
        return ESP_OK;  // 이미 등록됨
    }

    // 현재 등록된 디바이스 목록 로드
    config_registered_devices_t devices;
    esp_err_t ret = getRegisteredDevices(&devices);
    if (ret != ESP_OK) {
        return ret;
    }

    // 용량 초과 확인
    if (devices.count >= CONFIG_MAX_REGISTERED_DEVICES) {
        T_LOGE(TAG, "등록된 디바이스 수 초과: %d", devices.count);
        return ESP_ERR_NO_MEM;
    }

    // 디바이스 추가
    memcpy(devices.device_ids[devices.count], device_id, LORA_DEVICE_ID_LEN);
    devices.count++;

    // NVS 저장
    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS 열기 실패");
        return ESP_FAIL;
    }

    nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, devices.count);

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_PREFIX, devices.count - 1);
    nvs_set_blob(handle, key, devices.device_ids[devices.count - 1], LORA_DEVICE_ID_LEN);

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        char id_str[5];
        device_id_to_str(device_id, id_str);
        T_LOGI(TAG, "디바이스 등록: %s (%d/%d)",
               id_str, devices.count, CONFIG_MAX_REGISTERED_DEVICES);
    }

    return ret;
}

esp_err_t ConfigServiceClass::unregisterDevice(const uint8_t* device_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

    // 현재 등록된 디바이스 목록 로드
    config_registered_devices_t devices;
    esp_err_t ret = getRegisteredDevices(&devices);
    if (ret != ESP_OK) {
        return ret;
    }

    // 디바이스 찾기
    int found_idx = -1;
    for (uint8_t i = 0; i < devices.count; i++) {
        if (device_id_equals(devices.device_ids[i], device_id)) {
            found_idx = i;
            break;
        }
    }

    if (found_idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // 마지막 디바이스를 삭제된 위치로 이동 (빈틈 없애기)
    if (found_idx < devices.count - 1) {
        memcpy(devices.device_ids[found_idx],
               devices.device_ids[devices.count - 1],
               LORA_DEVICE_ID_LEN);
    }

    devices.count--;

    // NVS 저장
    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, devices.count);

    // 모든 디바이스 다시 저장
    for (uint8_t i = 0; i < devices.count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_PREFIX, i);
        nvs_set_blob(handle, key, devices.device_ids[i], LORA_DEVICE_ID_LEN);
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        char id_str[5];
        device_id_to_str(device_id, id_str);
        T_LOGI(TAG, "디바이스 등록 해제: %s", id_str);
    }

    return ret;
}

bool ConfigServiceClass::isDeviceRegistered(const uint8_t* device_id)
{
    if (!device_id) {
        return false;
    }

    config_registered_devices_t devices;
    if (getRegisteredDevices(&devices) != ESP_OK) {
        return false;
    }

    for (uint8_t i = 0; i < devices.count; i++) {
        if (device_id_equals(devices.device_ids[i], device_id)) {
            return true;
        }
    }
    return false;
}

esp_err_t ConfigServiceClass::getRegisteredDevices(config_registered_devices_t* devices)
{
    if (!devices) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(devices, 0, sizeof(config_registered_devices_t));

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        // NVS 없으면 빈 목록 반환
        return ESP_OK;
    }

    uint8_t count = 0;
    nvs_get_u8(handle, NVS_KEY_DEVICE_COUNT, &count);

    if (count > CONFIG_MAX_REGISTERED_DEVICES) {
        count = CONFIG_MAX_REGISTERED_DEVICES;
    }

    devices->count = 0;
    for (uint8_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_PREFIX, i);

        size_t len = LORA_DEVICE_ID_LEN;
        ret = nvs_get_blob(handle, key, devices->device_ids[i], &len);
        if (ret == ESP_OK && len == LORA_DEVICE_ID_LEN) {
            devices->count++;
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

uint8_t ConfigServiceClass::getRegisteredDeviceCount(void)
{
    config_registered_devices_t devices;
    if (getRegisteredDevices(&devices) != ESP_OK) {
        return 0;
    }
    return devices.count;
}

void ConfigServiceClass::clearRegisteredDevices(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    T_LOGI(TAG, "등록된 모든 디바이스 삭제");
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
// 기존 API
// ============================================================================

bool config_service_is_initialized(void)
{
    return ConfigServiceClass::isInitialized();
}

// ============================================================================
// Switcher 설정 API
// ============================================================================

esp_err_t config_service_get_primary(config_switcher_t* config)
{
    return ConfigServiceClass::getPrimary(config);
}

esp_err_t config_service_set_primary(const config_switcher_t* config)
{
    return ConfigServiceClass::setPrimary(config);
}

esp_err_t config_service_get_secondary(config_switcher_t* config)
{
    return ConfigServiceClass::getSecondary(config);
}

esp_err_t config_service_set_secondary(const config_switcher_t* config)
{
    return ConfigServiceClass::setSecondary(config);
}

bool config_service_get_dual_enabled(void)
{
    return ConfigServiceClass::getDualEnabled();
}

esp_err_t config_service_set_dual_enabled(bool enabled)
{
    return ConfigServiceClass::setDualEnabled(enabled);
}

uint8_t config_service_get_secondary_offset(void)
{
    return ConfigServiceClass::getSecondaryOffset();
}

esp_err_t config_service_set_secondary_offset(uint8_t offset)
{
    return ConfigServiceClass::setSecondaryOffset(offset);
}

// ============================================================================
// LED 색상 설정 API
// ============================================================================

esp_err_t config_service_get_led_colors(config_led_colors_t* config)
{
    return ConfigServiceClass::getLedColors(config);
}

esp_err_t config_service_set_led_colors(const config_led_colors_t* config)
{
    return ConfigServiceClass::setLedColors(config);
}

void config_service_get_led_program_color(uint8_t* r, uint8_t* g, uint8_t* b)
{
    ConfigServiceClass::getLedProgramColor(r, g, b);
}

void config_service_get_led_preview_color(uint8_t* r, uint8_t* g, uint8_t* b)
{
    ConfigServiceClass::getLedPreviewColor(r, g, b);
}

void config_service_get_led_off_color(uint8_t* r, uint8_t* g, uint8_t* b)
{
    ConfigServiceClass::getLedOffColor(r, g, b);
}

void config_service_get_led_battery_low_color(uint8_t* r, uint8_t* g, uint8_t* b)
{
    ConfigServiceClass::getLedBatteryLowColor(r, g, b);
}

// ============================================================================
// 등록된 디바이스 관리 API
// ============================================================================

esp_err_t config_service_register_device(const uint8_t* device_id)
{
    return ConfigServiceClass::registerDevice(device_id);
}

esp_err_t config_service_unregister_device(const uint8_t* device_id)
{
    return ConfigServiceClass::unregisterDevice(device_id);
}

bool config_service_is_device_registered(const uint8_t* device_id)
{
    return ConfigServiceClass::isDeviceRegistered(device_id);
}

esp_err_t config_service_get_registered_devices(config_registered_devices_t* devices)
{
    return ConfigServiceClass::getRegisteredDevices(devices);
}

uint8_t config_service_get_registered_device_count(void)
{
    return ConfigServiceClass::getRegisteredDeviceCount();
}

void config_service_clear_registered_devices(void)
{
    ConfigServiceClass::clearRegisteredDevices();
}

} // extern "C"
