/**
 * @file ConfigService.cpp
 * @brief NVS 설정 관리 서비스 구현 (C++)
 */

#include "ConfigService.h"
#include "NetworkConfig.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>

static const char* TAG = "ConfigService";

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
        ESP_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Config Service 초기화 중...");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_initialized = true;

    ESP_LOGI(TAG, "Config Service 초기화 완료");
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
        ESP_LOGW(TAG, "WiFi AP 설정 로드 실패, 기본값 사용");
        loadDefaults(config);
        return ret;
    }

    getWiFiSTA(&config->wifi_sta);
    getEthernet(&config->ethernet);

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
    config->wifi_ap.enabled = true;

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

    ESP_LOGI(TAG, "기본값 로드됨");
    return ESP_OK;
}

esp_err_t ConfigServiceClass::factoryReset(void)
{
    ESP_LOGI(TAG, "공장 초기화 수행 중...");

    config_all_t defaultConfig;
    loadDefaults(&defaultConfig);

    return saveAll(&defaultConfig);
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

bool config_service_is_initialized(void)
{
    return ConfigServiceClass::isInitialized();
}

} // extern "C"
