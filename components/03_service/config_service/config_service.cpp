/**
 * @file ConfigService.cpp
 * @brief NVS 설정 관리 서비스 구현 (C++)
 */

#include "config_service.h"
#include "NVSConfig.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "license_service.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "03_Config";

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
    static esp_err_t applyDeviceLimit(void);  // device_limit 적용 (초과분 삭제)

    // 전체 설정 로드/저장
    static esp_err_t loadAll(config_all_t* config);
    static esp_err_t saveAll(const config_all_t* config);

    // WiFi AP 설정
    static esp_err_t getWiFiAP(config_wifi_ap_t* config);
    static esp_err_t setWiFiAP(const config_wifi_ap_t* config);
    static esp_err_t setWiFiAPInternal(const config_wifi_ap_t* config);  // 내부용 (이벤트 미발행)

    // WiFi STA 설정
    static esp_err_t getWiFiSTA(config_wifi_sta_t* config);
    static esp_err_t setWiFiSTA(const config_wifi_sta_t* config);
    static esp_err_t setWiFiSTAInternal(const config_wifi_sta_t* config);  // 내부용 (이벤트 미발행)

    // Ethernet 설정
    static esp_err_t getEthernet(config_ethernet_t* config);
    static esp_err_t setEthernet(const config_ethernet_t* config);
    static esp_err_t setEthernetInternal(const config_ethernet_t* config);  // 내부용 (이벤트 미발행)

    // Device 설정
    static esp_err_t getDevice(config_device_t* config, int chip_type);
    static esp_err_t setDevice(const config_device_t* config);
    static esp_err_t setBrightness(uint8_t brightness);
    static esp_err_t setBrightnessInternal(uint8_t brightness);  // 이벤트 핸들러용 (이벤트 미발행)
    static esp_err_t setCameraId(uint8_t camera_id);
    static esp_err_t setCameraIdInternal(uint8_t camera_id);  // 이벤트 핸들러용 (이벤트 미발행)
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
    static esp_err_t setLedColorsInternal(const config_led_colors_t* config);
    static void getLedProgramColor(uint8_t* r, uint8_t* g, uint8_t* b);
    static void getLedPreviewColor(uint8_t* r, uint8_t* g, uint8_t* b);
    static void getLedOffColor(uint8_t* r, uint8_t* g, uint8_t* b);

    // 라이센스 데이터 (NVS "license" 네임스페이스)
    static esp_err_t getLicenseData(uint8_t* device_limit, char* key);
    static esp_err_t setLicenseData(uint8_t device_limit, const char* key);
    static esp_err_t setLicenseDataInternal(const license_data_event_t* data);

    // 등록된 디바이스 관리
    static esp_err_t registerDevice(const uint8_t* device_id);
    static esp_err_t unregisterDevice(const uint8_t* device_id);
    static bool isDeviceRegistered(const uint8_t* device_id);
    static esp_err_t getRegisteredDevices(config_registered_devices_t* devices);
    static uint8_t getRegisteredDeviceCount(void);
    static void clearRegisteredDevices(void);

    // 디바이스 카메라 ID 매핑 (TX 장치 기억)
    static esp_err_t setDeviceCameraId(const uint8_t* device_id, uint8_t camera_id);
    static esp_err_t getDeviceCameraId(const uint8_t* device_id, uint8_t* camera_id);
    static esp_err_t getDeviceCamMap(config_device_cam_map_t* map);
    static esp_err_t removeDeviceCamMap(const uint8_t* device_id);
    static void clearDeviceCamMap(void);

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
    static config_device_cam_map_t s_device_cam_map;  // 디바이스-카메라 매핑 캐시
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool ConfigServiceClass::s_initialized = false;
config_device_cam_map_t ConfigServiceClass::s_device_cam_map = {
    .device_ids = {0},
    .camera_ids = {0},
    .count = 0
};

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

/**
 * @brief 설정 저장 요청 이벤트 핸들러
 * web_server에서 EVT_CONFIG_CHANGED로 발행하는 설정 저장 요청 처리
 */
static esp_err_t on_config_save_request(const event_data_t* event) {
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const auto* req = reinterpret_cast<const config_save_request_t*>(event->data);
    esp_err_t ret = ESP_OK;

    switch (req->type) {
        case CONFIG_SAVE_WIFI_AP: {
            config_wifi_ap_t ap = {};
            strncpy(ap.ssid, req->wifi_ap_ssid, sizeof(ap.ssid) - 1);
            strncpy(ap.password, req->wifi_ap_password, sizeof(ap.password) - 1);
            ap.channel = req->wifi_ap_channel;
            ap.enabled = req->wifi_ap_enabled;
            ret = ConfigServiceClass::setWiFiAP(&ap);
            break;
        }

        case CONFIG_SAVE_WIFI_STA: {
            config_wifi_sta_t sta = {};
            strncpy(sta.ssid, req->wifi_sta_ssid, sizeof(sta.ssid) - 1);
            strncpy(sta.password, req->wifi_sta_password, sizeof(sta.password) - 1);
            sta.enabled = req->wifi_sta_enabled;
            ret = ConfigServiceClass::setWiFiSTA(&sta);
            break;
        }

        case CONFIG_SAVE_ETHERNET: {
            config_ethernet_t eth = {};
            eth.dhcp_enabled = req->eth_dhcp;
            strncpy(eth.static_ip, req->eth_static_ip, sizeof(eth.static_ip) - 1);
            strncpy(eth.static_netmask, req->eth_netmask, sizeof(eth.static_netmask) - 1);
            strncpy(eth.static_gateway, req->eth_gateway, sizeof(eth.static_gateway) - 1);
            eth.enabled = req->eth_enabled;
            ret = ConfigServiceClass::setEthernet(&eth);
            break;
        }

        case CONFIG_SAVE_SWITCHER_PRIMARY: {
            config_switcher_t sw = {};
            // switcher_type 문자열을 숫자로 변환
            if (strcmp(req->switcher_type, "ATEM") == 0) {
                sw.type = 0;
            } else if (strcmp(req->switcher_type, "OBS") == 0) {
                sw.type = 1;
            } else {
                sw.type = 2;  // vMix
            }
            strncpy(sw.ip, req->switcher_ip, sizeof(sw.ip) - 1);
            sw.port = req->switcher_port;
            sw.interface = req->switcher_interface;
            sw.camera_limit = req->switcher_camera_limit;
            strncpy(sw.password, req->switcher_password, sizeof(sw.password) - 1);
            ret = ConfigServiceClass::setPrimary(&sw);
            break;
        }

        case CONFIG_SAVE_SWITCHER_SECONDARY: {
            config_switcher_t sw = {};
            // switcher_type 문자열을 숫자로 변환
            if (strcmp(req->switcher_type, "ATEM") == 0) {
                sw.type = 0;
            } else if (strcmp(req->switcher_type, "OBS") == 0) {
                sw.type = 1;
            } else {
                sw.type = 2;  // vMix
            }
            strncpy(sw.ip, req->switcher_ip, sizeof(sw.ip) - 1);
            sw.port = req->switcher_port;
            sw.interface = req->switcher_interface;
            sw.camera_limit = req->switcher_camera_limit;
            strncpy(sw.password, req->switcher_password, sizeof(sw.password) - 1);
            ret = ConfigServiceClass::setSecondary(&sw);
            break;
        }

        case CONFIG_SAVE_SWITCHER_DUAL:
            // 듀얼 모드 설정 (항상 저장, false일 때도 호출)
            T_LOGI(TAG, "Saving dual mode: enabled=%d, offset=%d",
                     req->switcher_dual_enabled, req->switcher_secondary_offset);
            ret = ConfigServiceClass::setDualEnabled(req->switcher_dual_enabled);
            if (ret == ESP_OK) {
                // Secondary Offset 저장 (0도 유효한 값)
                ret = ConfigServiceClass::setSecondaryOffset(req->switcher_secondary_offset);
            }
            break;

        case CONFIG_SAVE_DEVICE_BRIGHTNESS:
            ret = ConfigServiceClass::setBrightness(req->brightness);
            break;

        case CONFIG_SAVE_DEVICE_CAMERA_ID:
            ret = ConfigServiceClass::setCameraId(req->camera_id);
            break;

        case CONFIG_SAVE_DEVICE_RF:
            ret = ConfigServiceClass::setRf(req->rf_frequency, req->rf_sync_word);
            break;

        default:
            T_LOGW(TAG, "Unknown config save type: %d", req->type);
            return ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        T_LOGD(TAG, "Config saved via event: type=%d", req->type);

        // 저장 완료 후 전체 설정 데이터를 로드하여 stack 변수에 저장
        config_all_t full_config;
        if (ConfigServiceClass::loadAll(&full_config) == ESP_OK) {
            // stack 변수 사용 (이벤트 버스가 복사)
            config_data_event_t data_event = {};
            memset(&data_event, 0, sizeof(config_data_event_t));

            // WiFi AP
            strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
            data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
            strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
            data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
            data_event.wifi_ap_channel = full_config.wifi_ap.channel;
            data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

            // WiFi STA
            strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
            data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
            strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
            data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
            data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

            // Ethernet
            data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
            strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
            data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
            strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
            data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
            strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
            data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
            data_event.eth_enabled = full_config.ethernet.enabled;

            // Device
            data_event.device_brightness = full_config.device.brightness;
            data_event.device_camera_id = full_config.device.camera_id;
            data_event.device_rf_frequency = full_config.device.rf.frequency;
            data_event.device_rf_sync_word = full_config.device.rf.sync_word;
            data_event.device_rf_sf = full_config.device.rf.sf;
            data_event.device_rf_cr = full_config.device.rf.cr;
            data_event.device_rf_bw = full_config.device.rf.bw;
            data_event.device_rf_tx_power = full_config.device.rf.tx_power;

            // Switcher Primary
            data_event.primary_type = full_config.primary.type;
            strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
            data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
            data_event.primary_port = full_config.primary.port;
            data_event.primary_interface = full_config.primary.interface;
            data_event.primary_camera_limit = full_config.primary.camera_limit;
            strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
            data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

            // Switcher Secondary
            data_event.secondary_type = full_config.secondary.type;
            strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
            data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
            data_event.secondary_port = full_config.secondary.port;
            data_event.secondary_interface = full_config.secondary.interface;
            data_event.secondary_camera_limit = full_config.secondary.camera_limit;
            strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
            data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

            // Switcher Dual
            data_event.dual_enabled = full_config.dual_enabled;
            data_event.secondary_offset = full_config.secondary_offset;

            // stack 변수 발행 (이벤트 버스가 복사)
            event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
        }
    } else {
        T_LOGE(TAG, "Config save failed: type=%d", req->type);
    }

    return ret;
}

/**
 * @brief 설정 데이터 요청 이벤트 핸들러
 * web_server에서 EVT_CONFIG_DATA_REQUEST로 발행하는 설정 데이터 요청 처리
 * 현재 설정을 로드하여 stack 변수에 저장 후 발행
 */
static esp_err_t on_config_data_request(const event_data_t* event)
{
    (void)event;  // 데이터 없음

    T_LOGI(TAG, "Config data request received");

    // 현재 설정 로드
    config_all_t full_config;
    if (ConfigServiceClass::loadAll(&full_config) != ESP_OK) {
        T_LOGE(TAG, "Failed to load config for request");
        return ESP_FAIL;
    }

    // stack 변수 사용 (이벤트 버스가 복사)
    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(config_data_event_t));

    // WiFi AP
    strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = full_config.wifi_ap.channel;
    data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

    // WiFi STA
    strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

    // Ethernet
    data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = full_config.ethernet.enabled;

    // Device
    data_event.device_brightness = full_config.device.brightness;
    data_event.device_camera_id = full_config.device.camera_id;
    data_event.device_rf_frequency = full_config.device.rf.frequency;
    data_event.device_rf_sync_word = full_config.device.rf.sync_word;
    data_event.device_rf_sf = full_config.device.rf.sf;
    data_event.device_rf_cr = full_config.device.rf.cr;
    data_event.device_rf_bw = full_config.device.rf.bw;
    data_event.device_rf_tx_power = full_config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = full_config.primary.type;
    strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = full_config.primary.port;
    data_event.primary_interface = full_config.primary.interface;
    data_event.primary_camera_limit = full_config.primary.camera_limit;
    strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = full_config.secondary.type;
    strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = full_config.secondary.port;
    data_event.secondary_interface = full_config.secondary.interface;
    data_event.secondary_camera_limit = full_config.secondary.camera_limit;
    strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = full_config.dual_enabled;
    data_event.secondary_offset = full_config.secondary_offset;

    // stack 변수 발행 (이벤트 버스가 복사)
    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));

    return ESP_OK;
}

// ============================================================================
// RF 저장 이벤트 핸들러 (TX/RX 공용)
// TX broadcast 완료 후 또는 RX 수신 시 NVS에 저장
// ============================================================================

static esp_err_t on_rf_saved(const event_data_t* event) {
    if (event->type != EVT_RF_SAVED && event->type != EVT_RF_CHANGED) {
        return ESP_OK;
    }

    const auto* rf = reinterpret_cast<const lora_rf_event_t*>(event->data);
    if (rf == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // 현재 설정 로드
    config_all_t config;
    esp_err_t ret = ConfigServiceClass::loadAll(&config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "RF config save failed: load failed");
        return ret;
    }

    // RF 설정 업데이트
    config.device.rf.frequency = rf->frequency;
    config.device.rf.sync_word = rf->sync_word;

    // NVS에 저장
    ret = ConfigServiceClass::saveAll(&config);
    if (ret == ESP_OK) {
        T_LOGD(TAG, "RF config saved: %.1f MHz, Sync 0x%02X (NVS)",
               rf->frequency, rf->sync_word);
    } else {
        T_LOGE(TAG, "RF config NVS save failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 설정 변경 후 전체 데이터 이벤트 발행 (web_server 캐시 갱신)
    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(data_event));

    // WiFi AP
    strncpy(data_event.wifi_ap_ssid, config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = config.wifi_ap.channel;
    data_event.wifi_ap_enabled = config.wifi_ap.enabled;

    // WiFi STA
    strncpy(data_event.wifi_sta_ssid, config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = config.wifi_sta.enabled;

    // Ethernet
    data_event.eth_dhcp_enabled = config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = config.ethernet.enabled;

    // Device
    data_event.device_brightness = config.device.brightness;
    data_event.device_camera_id = config.device.camera_id;
    data_event.device_rf_frequency = config.device.rf.frequency;
    data_event.device_rf_sync_word = config.device.rf.sync_word;
    data_event.device_rf_sf = config.device.rf.sf;
    data_event.device_rf_cr = config.device.rf.cr;
    data_event.device_rf_bw = config.device.rf.bw;
    data_event.device_rf_tx_power = config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = config.primary.type;
    strncpy(data_event.primary_ip, config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = config.primary.port;
    data_event.primary_interface = config.primary.interface;
    data_event.primary_camera_limit = config.primary.camera_limit;
    strncpy(data_event.primary_password, config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = config.secondary.type;
    strncpy(data_event.secondary_ip, config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = config.secondary.port;
    data_event.secondary_interface = config.secondary.interface;
    data_event.secondary_camera_limit = config.secondary.camera_limit;
    strncpy(data_event.secondary_password, config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = config.dual_enabled;
    data_event.secondary_offset = config.secondary_offset;

    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));

    return ESP_OK;
}

/**
 * @brief 카메라 ID 변경 이벤트 핸들러 (LoRa 수신 시 NVS 저장)
 */
static esp_err_t on_camera_id_changed(const event_data_t* event) {
    if (event->type != EVT_CAMERA_ID_CHANGED) {
        return ESP_OK;
    }

    if (event->data_size < sizeof(uint8_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* camera_id = reinterpret_cast<const uint8_t*>(event->data);

    // 내부 함수 호출 (이벤트 미발행으로 무한 루프 방지)
    esp_err_t ret = ConfigServiceClass::setCameraIdInternal(*camera_id);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "camera_id saved (LoRa rx): %d (NVS)", *camera_id);

        // 설정 변경 후 전체 데이터 이벤트 발행
        config_data_event_t data_event = {};
        memset(&data_event, 0, sizeof(data_event));
        data_event.device_camera_id = *camera_id;
        event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    } else {
        T_LOGE(TAG, "camera_id NVS save failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 밝기 변경 이벤트 핸들러 (LoRa 수신 시 NVS 저장)
 */
static esp_err_t on_brightness_changed(const event_data_t* event) {
    if (event->type != EVT_BRIGHTNESS_CHANGED) {
        return ESP_OK;
    }

    if (event->data_size < sizeof(uint8_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* brightness = reinterpret_cast<const uint8_t*>(event->data);

    // 내부 함수 호출 (이벤트 미발행으로 무한 루프 방지)
    esp_err_t ret = ConfigServiceClass::setBrightnessInternal(*brightness);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "brightness saved (LoRa rx): %d (NVS)", *brightness);

        // 설정 변경 후 전체 데이터 이벤트 발행
        config_data_event_t data_event = {};
        memset(&data_event, 0, sizeof(data_event));
        data_event.device_brightness = *brightness;
        event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    } else {
        T_LOGE(TAG, "brightness NVS save failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief LED 색상 변경 이벤트 핸들러 (웹에서 변경 시 NVS 저장 + LoRa 브로드캐스트)
 */
static esp_err_t on_led_colors_changed(const event_data_t* event)
{
    if (event->type != EVT_LED_COLORS_CHANGED) {
        return ESP_OK;
    }

    if (event->data_size < sizeof(led_colors_event_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const led_colors_event_t* colors = reinterpret_cast<const led_colors_event_t*>(event->data);

    // NVS 저장 (내부 함수 호출로 무한 루프 방지)
    config_led_colors_t nvs_colors = {
        .program = { .r = colors->program_r, .g = colors->program_g, .b = colors->program_b },
        .preview = { .r = colors->preview_r, .g = colors->preview_g, .b = colors->preview_b },
        .off = { .r = colors->off_r, .g = colors->off_g, .b = colors->off_b }
    };

    esp_err_t ret = ConfigServiceClass::setLedColorsInternal(&nvs_colors);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "LED colors saved: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d)",
                 colors->program_r, colors->program_g, colors->program_b,
                 colors->preview_r, colors->preview_g, colors->preview_b,
                 colors->off_r, colors->off_g, colors->off_b);

        // LoRa 브로드캐스트 (TX에서만 송신)
#ifdef DEVICE_MODE_TX
        lora_cmd_led_colors_t led_cmd = {
            .header = LORA_HDR_LED_COLORS,
            .program_r = colors->program_r,
            .program_g = colors->program_g,
            .program_b = colors->program_b,
            .preview_r = colors->preview_r,
            .preview_g = colors->preview_g,
            .preview_b = colors->preview_b,
            .off_r = colors->off_r,
            .off_g = colors->off_g,
            .off_b = colors->off_b
        };
        event_bus_publish(EVT_DEVICE_LED_COLORS_REQUEST, &led_cmd, sizeof(led_cmd));
#endif
    } else {
        T_LOGE(TAG, "LED colors NVS save failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief LED 색상 조회 요청 이벤트 핸들러 (web_server에서 발행)
 * NVS에서 색상을 읽어서 EVT_LED_COLORS_CHANGED로 응답
 */
static esp_err_t on_led_colors_request(const event_data_t* event)
{
    if (event->type != EVT_LED_COLORS_REQUEST) {
        return ESP_OK;
    }

    // NVS에서 색상 읽기
    config_led_colors_t colors;
    esp_err_t ret = ConfigServiceClass::getLedColors(&colors);

    if (ret == ESP_OK) {
        // 응답으로 색상 이벤트 발행
        static led_colors_event_t response;
        response.program_r = colors.program.r;
        response.program_g = colors.program.g;
        response.program_b = colors.program.b;
        response.preview_r = colors.preview.r;
        response.preview_g = colors.preview.g;
        response.preview_b = colors.preview.b;
        response.off_r = colors.off.r;
        response.off_g = colors.off.g;
        response.off_b = colors.off.b;

        event_bus_publish(EVT_LED_COLORS_CHANGED, &response, sizeof(response));
    }

    return ret;
}

/**
 * @brief 라이센스 데이터 저장 이벤트 핸들러
 * @note license_service에서 발행, NVS에 저장
 */
static esp_err_t on_license_data_save(const event_data_t* event)
{
    if (event->type != EVT_LICENSE_DATA_SAVE) {
        return ESP_OK;
    }

    if (event->data_size < sizeof(license_data_event_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const license_data_event_t* data = reinterpret_cast<const license_data_event_t*>(event->data);

    // NVS 저장 (내부 함수 호출로 무한 루프 방지)
    esp_err_t ret = ConfigServiceClass::setLicenseDataInternal(data);
    if (ret == ESP_OK) {
        T_LOGD(TAG, "license data saved: limit=%d, key=%.16s", data->device_limit, data->key);
    } else {
        T_LOGE(TAG, "license data NVS save failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 라이센스 데이터 조회 요청 이벤트 핸들러
 * @note license_service에서 발행, NVS에서 읽어서 EVT_LICENSE_DATA_SAVE로 응답
 */
static esp_err_t on_license_data_request(const event_data_t* event)
{
    if (event->type != EVT_LICENSE_DATA_REQUEST) {
        return ESP_OK;
    }

    // NVS에서 라이센스 데이터 읽기
    uint8_t device_limit = 0;
    char key[17] = {0};
    esp_err_t ret = ConfigServiceClass::getLicenseData(&device_limit, key);

    if (ret == ESP_OK) {
        // 응답으로 라이센스 데이터 이벤트 발행
        license_data_event_t response;
        response.device_limit = device_limit;
        strncpy(response.key, key, 16);
        response.key[16] = '\0';

        event_bus_publish(EVT_LICENSE_DATA_SAVE, &response, sizeof(response));
    }

    return ret;
}

/**
 * @brief 공장 초기화 요청 이벤트 핸들러
 * @note 웹 API에서 발행
 */
static esp_err_t on_factory_reset_request(const event_data_t* event)
{
    if (event->type != EVT_FACTORY_RESET_REQUEST) {
        return ESP_OK;
    }

    T_LOGI(TAG, "Factory reset request received via event bus");

    // Factory reset 실행
    esp_err_t ret = ConfigServiceClass::factoryReset();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    T_LOGI(TAG, "Factory reset successful, rebooting in 1 second...");

    // 1초 대기 후 재부팅
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

/**
 * @brief 디바이스 카메라 매핑 수신 이벤트 핸들러
 * @note 상태 응답 수신 시 device_manager에서 발행
 */
static esp_err_t on_device_cam_map_receive(const event_data_t* event)
{
    if (event->type != EVT_DEVICE_CAM_MAP_RECEIVE) {
        return ESP_OK;
    }

    if (event->data_size < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(event->data);
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t camera_id = data[2];

    // NVS에 저장
    esp_err_t ret = ConfigServiceClass::setDeviceCameraId(device_id, camera_id);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "device-camera map NVS save failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 디바이스 카메라 매핑 로드 요청 이벤트 핸들러
 * @note 저장된 모든 매핑을 개별 이벤트로 발행
 */
static esp_err_t on_device_cam_map_load(const event_data_t* event)
{
    if (event->type != EVT_DEVICE_CAM_MAP_LOAD) {
        return ESP_OK;
    }

    // NVS에서 모든 매핑 로드
    config_device_cam_map_t map;
    esp_err_t ret = ConfigServiceClass::getDeviceCamMap(&map);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "device-camera map load failed: %s", esp_err_to_name(ret));
        return ret;
    }

    T_LOGI(TAG, "device-camera map loaded: %d", map.count);

    // 각 매핑을 개별 이벤트로 발행
    for (uint8_t i = 0; i < map.count; i++) {
        uint8_t data[3] = {map.device_ids[i][0], map.device_ids[i][1], map.camera_ids[i]};
        event_bus_publish(EVT_DEVICE_CAM_MAP_RECEIVE, data, sizeof(data));
    }

    return ESP_OK;
}

// ============================================================================
// 전체 설정 이벤트 발행 헬퍼
// ============================================================================

/**
 * @brief NVS에서 전체 설정을 로드하여 이벤트 발행
 * @note WiFi/네트워크 설정 변경 후 전체 설정을 재발행하여 다른 서비스 동기화
 */
static void publish_full_config_event(void)
{
    config_all_t full_config;
    if (ConfigServiceClass::loadAll(&full_config) != ESP_OK) {
        T_LOGW(TAG, "full config load failed, event publish skipped");
        return;
    }

    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(config_data_event_t));

    // WiFi AP
    strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = full_config.wifi_ap.channel;
    data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

    // WiFi STA
    strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

    // Ethernet
    data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = full_config.ethernet.enabled;

    // Device
    data_event.device_brightness = full_config.device.brightness;
    data_event.device_camera_id = full_config.device.camera_id;
    data_event.device_rf_frequency = full_config.device.rf.frequency;
    data_event.device_rf_sync_word = full_config.device.rf.sync_word;
    data_event.device_rf_sf = full_config.device.rf.sf;
    data_event.device_rf_cr = full_config.device.rf.cr;
    data_event.device_rf_bw = full_config.device.rf.bw;
    data_event.device_rf_tx_power = full_config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = full_config.primary.type;
    strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = full_config.primary.port;
    data_event.primary_interface = full_config.primary.interface;
    data_event.primary_camera_limit = full_config.primary.camera_limit;
    strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = full_config.secondary.type;
    strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = full_config.secondary.port;
    data_event.secondary_interface = full_config.secondary.interface;
    data_event.secondary_camera_limit = full_config.secondary.camera_limit;
    strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = full_config.dual_enabled;
    data_event.secondary_offset = full_config.secondary_offset;

    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    T_LOGD(TAG, "full config data event published");
}

/**
 * @brief WiFi STA 설정을 파라미터로 받아 전체 설정 이벤트 발행
 * @param sta_config 변경된 WiFi STA 설정 (NULL이면 NVS에서 로드)
 * @note NVS 커밋 타이밍 문제 회피 위해 파라미터 값을 직접 사용
 */
static void publish_config_event_with_sta(const config_wifi_sta_t* sta_config)
{
    config_all_t full_config;
    if (ConfigServiceClass::loadAll(&full_config) != ESP_OK) {
        T_LOGW(TAG, "full config load failed, event publish skipped");
        return;
    }

    // 파라미터로 받은 STA 설정 사용 (NVS 커밋 타이밍 문제 회피)
    if (sta_config) {
        full_config.wifi_sta = *sta_config;
    }

    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(config_data_event_t));

    // WiFi AP
    strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = full_config.wifi_ap.channel;
    data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

    // WiFi STA (파라미터 값 사용)
    strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

    // Ethernet
    data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = full_config.ethernet.enabled;

    // Device
    data_event.device_brightness = full_config.device.brightness;
    data_event.device_camera_id = full_config.device.camera_id;
    data_event.device_rf_frequency = full_config.device.rf.frequency;
    data_event.device_rf_sync_word = full_config.device.rf.sync_word;
    data_event.device_rf_sf = full_config.device.rf.sf;
    data_event.device_rf_cr = full_config.device.rf.cr;
    data_event.device_rf_bw = full_config.device.rf.bw;
    data_event.device_rf_tx_power = full_config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = full_config.primary.type;
    strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = full_config.primary.port;
    data_event.primary_interface = full_config.primary.interface;
    data_event.primary_camera_limit = full_config.primary.camera_limit;
    strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = full_config.secondary.type;
    strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = full_config.secondary.port;
    data_event.secondary_interface = full_config.secondary.interface;
    data_event.secondary_camera_limit = full_config.secondary.camera_limit;
    strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = full_config.dual_enabled;
    data_event.secondary_offset = full_config.secondary_offset;

    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    T_LOGD(TAG, "full config data event published (STA: enabled=%d)", full_config.wifi_sta.enabled);
}

/**
 * @brief WiFi AP 설정을 파라미터로 받아 전체 설정 이벤트 발행
 * @param ap_config 변경된 WiFi AP 설정 (NULL이면 NVS에서 로드)
 */
static void publish_config_event_with_ap(const config_wifi_ap_t* ap_config)
{
    config_all_t full_config;
    if (ConfigServiceClass::loadAll(&full_config) != ESP_OK) {
        T_LOGW(TAG, "full config load failed, event publish skipped");
        return;
    }

    // 파라미터로 받은 AP 설정 사용
    if (ap_config) {
        full_config.wifi_ap = *ap_config;
    }

    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(config_data_event_t));

    // WiFi AP (파라미터 값 사용)
    strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = full_config.wifi_ap.channel;
    data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

    // WiFi STA
    strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

    // Ethernet
    data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = full_config.ethernet.enabled;

    // Device
    data_event.device_brightness = full_config.device.brightness;
    data_event.device_camera_id = full_config.device.camera_id;
    data_event.device_rf_frequency = full_config.device.rf.frequency;
    data_event.device_rf_sync_word = full_config.device.rf.sync_word;
    data_event.device_rf_sf = full_config.device.rf.sf;
    data_event.device_rf_cr = full_config.device.rf.cr;
    data_event.device_rf_bw = full_config.device.rf.bw;
    data_event.device_rf_tx_power = full_config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = full_config.primary.type;
    strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = full_config.primary.port;
    data_event.primary_interface = full_config.primary.interface;
    data_event.primary_camera_limit = full_config.primary.camera_limit;
    strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = full_config.secondary.type;
    strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = full_config.secondary.port;
    data_event.secondary_interface = full_config.secondary.interface;
    data_event.secondary_camera_limit = full_config.secondary.camera_limit;
    strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = full_config.dual_enabled;
    data_event.secondary_offset = full_config.secondary_offset;

    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    T_LOGD(TAG, "full config data event published (AP: enabled=%d)", full_config.wifi_ap.enabled);
}

/**
 * @brief Ethernet 설정을 파라미터로 받아 전체 설정 이벤트 발행
 * @param eth_config 변경된 Ethernet 설정 (NULL이면 NVS에서 로드)
 */
static void publish_config_event_with_eth(const config_ethernet_t* eth_config)
{
    config_all_t full_config;
    if (ConfigServiceClass::loadAll(&full_config) != ESP_OK) {
        T_LOGW(TAG, "full config load failed, event publish skipped");
        return;
    }

    // 파라미터로 받은 Ethernet 설정 사용
    if (eth_config) {
        full_config.ethernet = *eth_config;
    }

    config_data_event_t data_event = {};
    memset(&data_event, 0, sizeof(config_data_event_t));

    // WiFi AP
    strncpy(data_event.wifi_ap_ssid, full_config.wifi_ap.ssid, sizeof(data_event.wifi_ap_ssid) - 1);
    data_event.wifi_ap_ssid[sizeof(data_event.wifi_ap_ssid) - 1] = '\0';
    strncpy(data_event.wifi_ap_password, full_config.wifi_ap.password, sizeof(data_event.wifi_ap_password) - 1);
    data_event.wifi_ap_password[sizeof(data_event.wifi_ap_password) - 1] = '\0';
    data_event.wifi_ap_channel = full_config.wifi_ap.channel;
    data_event.wifi_ap_enabled = full_config.wifi_ap.enabled;

    // WiFi STA
    strncpy(data_event.wifi_sta_ssid, full_config.wifi_sta.ssid, sizeof(data_event.wifi_sta_ssid) - 1);
    data_event.wifi_sta_ssid[sizeof(data_event.wifi_sta_ssid) - 1] = '\0';
    strncpy(data_event.wifi_sta_password, full_config.wifi_sta.password, sizeof(data_event.wifi_sta_password) - 1);
    data_event.wifi_sta_password[sizeof(data_event.wifi_sta_password) - 1] = '\0';
    data_event.wifi_sta_enabled = full_config.wifi_sta.enabled;

    // Ethernet (파라미터 값 사용)
    data_event.eth_dhcp_enabled = full_config.ethernet.dhcp_enabled;
    strncpy(data_event.eth_static_ip, full_config.ethernet.static_ip, sizeof(data_event.eth_static_ip) - 1);
    data_event.eth_static_ip[sizeof(data_event.eth_static_ip) - 1] = '\0';
    strncpy(data_event.eth_static_netmask, full_config.ethernet.static_netmask, sizeof(data_event.eth_static_netmask) - 1);
    data_event.eth_static_netmask[sizeof(data_event.eth_static_netmask) - 1] = '\0';
    strncpy(data_event.eth_static_gateway, full_config.ethernet.static_gateway, sizeof(data_event.eth_static_gateway) - 1);
    data_event.eth_static_gateway[sizeof(data_event.eth_static_gateway) - 1] = '\0';
    data_event.eth_enabled = full_config.ethernet.enabled;

    // Device
    data_event.device_brightness = full_config.device.brightness;
    data_event.device_camera_id = full_config.device.camera_id;
    data_event.device_rf_frequency = full_config.device.rf.frequency;
    data_event.device_rf_sync_word = full_config.device.rf.sync_word;
    data_event.device_rf_sf = full_config.device.rf.sf;
    data_event.device_rf_cr = full_config.device.rf.cr;
    data_event.device_rf_bw = full_config.device.rf.bw;
    data_event.device_rf_tx_power = full_config.device.rf.tx_power;

    // Switcher Primary
    data_event.primary_type = full_config.primary.type;
    strncpy(data_event.primary_ip, full_config.primary.ip, sizeof(data_event.primary_ip) - 1);
    data_event.primary_ip[sizeof(data_event.primary_ip) - 1] = '\0';
    data_event.primary_port = full_config.primary.port;
    data_event.primary_interface = full_config.primary.interface;
    data_event.primary_camera_limit = full_config.primary.camera_limit;
    strncpy(data_event.primary_password, full_config.primary.password, sizeof(data_event.primary_password) - 1);
    data_event.primary_password[sizeof(data_event.primary_password) - 1] = '\0';

    // Switcher Secondary
    data_event.secondary_type = full_config.secondary.type;
    strncpy(data_event.secondary_ip, full_config.secondary.ip, sizeof(data_event.secondary_ip) - 1);
    data_event.secondary_ip[sizeof(data_event.secondary_ip) - 1] = '\0';
    data_event.secondary_port = full_config.secondary.port;
    data_event.secondary_interface = full_config.secondary.interface;
    data_event.secondary_camera_limit = full_config.secondary.camera_limit;
    strncpy(data_event.secondary_password, full_config.secondary.password, sizeof(data_event.secondary_password) - 1);
    data_event.secondary_password[sizeof(data_event.secondary_password) - 1] = '\0';

    // Switcher Dual
    data_event.dual_enabled = full_config.dual_enabled;
    data_event.secondary_offset = full_config.secondary_offset;

    event_bus_publish(EVT_CONFIG_DATA_CHANGED, &data_event, sizeof(config_data_event_t));
    T_LOGD(TAG, "full config data event published (ETH: enabled=%d)", full_config.ethernet.enabled);
}

// ============================================================================
// 초기화
// ============================================================================

esp_err_t ConfigServiceClass::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    T_LOGI(TAG, "initializing...");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            T_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            T_LOGE(TAG, "NVS init failed after erase: %s", esp_err_to_name(ret));
            return ret;
        }
    } else if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    T_LOGI(TAG, "NVS init complete");

    T_LOGD(TAG, "event bus subscribe start...");
    // 디바이스 등록/해제 이벤트 구독
    event_bus_subscribe(EVT_DEVICE_REGISTER, on_device_register_request);
    event_bus_subscribe(EVT_DEVICE_UNREGISTER, on_device_unregister_request);
    // 설정 저장 요청 이벤트 구독 (web_server에서 발행)
    event_bus_subscribe(EVT_CONFIG_CHANGED, on_config_save_request);
    event_bus_subscribe(EVT_CONFIG_DATA_REQUEST, on_config_data_request);
    // RF 변경 이벤트 구독 (RX에서 RF 수신 시 NVS 저장)
    event_bus_subscribe(EVT_RF_CHANGED, on_rf_saved);
    // RF 저장 이벤트 구독 (TX broadcast 완료 후 NVS 저장)
    event_bus_subscribe(EVT_RF_SAVED, on_rf_saved);
    // 카메라 ID 변경 이벤트 구독 (LoRa 수신 시 NVS 저장)
    event_bus_subscribe(EVT_CAMERA_ID_CHANGED, on_camera_id_changed);
    // 밝기 변경 이벤트 구독 (LoRa 수신 시 NVS 저장)
    event_bus_subscribe(EVT_BRIGHTNESS_CHANGED, on_brightness_changed);
    // LED 색상 변경 이벤트 구독 (웹에서 변경 시 NVS 저장)
    event_bus_subscribe(EVT_LED_COLORS_CHANGED, on_led_colors_changed);
    // LED 색상 조회 요청 이벤트 구독 (웹에서 요청 시 NVS 읽어서 응답)
    event_bus_subscribe(EVT_LED_COLORS_REQUEST, on_led_colors_request);
    // 라이센스 데이터 저장 이벤트 구독 (license_service에서 발행, NVS 저장)
    event_bus_subscribe(EVT_LICENSE_DATA_SAVE, on_license_data_save);
    // 라이센스 데이터 조회 요청 이벤트 구독 (license_service에서 발행, NVS 읽어서 응답)
    event_bus_subscribe(EVT_LICENSE_DATA_REQUEST, on_license_data_request);
    // 디바이스 카메라 매핑 수신 이벤트 구독 (상태 응답 수신 시 NVS 저장)
    event_bus_subscribe(EVT_DEVICE_CAM_MAP_RECEIVE, on_device_cam_map_receive);
    // 디바이스 카메라 매핑 로드 요청 이벤트 구독 (TX 시작 시 NVS 매핑 로드)
    event_bus_subscribe(EVT_DEVICE_CAM_MAP_LOAD, on_device_cam_map_load);
    // 공장 초기화 요청 이벤트 구독
    event_bus_subscribe(EVT_FACTORY_RESET_REQUEST, on_factory_reset_request);
    T_LOGD(TAG, "event bus subscribe complete");

    s_initialized = true;

    T_LOGI(TAG, "init complete");

    // 초기화 완료 후 전체 설정 데이터 이벤트 발행
    // 다른 서비스들이 설정을 로드할 수 있도록 함
    publish_full_config_event();

    return ESP_OK;
}

// ============================================================================
// Device 설정 (TX 전용)
// ============================================================================

esp_err_t ConfigServiceClass::applyDeviceLimit(void)
{
    // getRegisteredDevices 호출 시 device_limit 체크 및 초과분 삭제 수행
    config_registered_devices_t devices;
    esp_err_t ret = getRegisteredDevices(&devices);
    if (ret == ESP_OK) {
        T_LOGD(TAG, "registered devices device_limit applied: %d", devices.count);
    }

    // getDeviceCamMap 호출 시 device_limit 체크 및 초과분 삭제 수행
    config_device_cam_map_t cam_map;
    ret = getDeviceCamMap(&cam_map);
    if (ret == ESP_OK) {
        T_LOGD(TAG, "device-camera map device_limit applied: %d", cam_map.count);
    }

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
        T_LOGW(TAG, "WiFi AP config load failed, using defaults");
        loadDefaults(config);
        return ret;
    }

    getWiFiSTA(&config->wifi_sta);
    getEthernet(&config->ethernet);
    getDevice(&config->device, 0);  // LORA_CHIP_UNKNOWN
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

    // 내부 함수 사용 (이벤트 미발행으로 중복 이벤트 방지)
    ret = setWiFiAPInternal(&config->wifi_ap);
    if (ret != ESP_OK) return ret;

    ret = setWiFiSTAInternal(&config->wifi_sta);
    if (ret != ESP_OK) return ret;

    ret = setEthernetInternal(&config->ethernet);
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
    // password: 빈 문자열이면 NVS 키 삭제, otherwise 저장
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_ap_pass", config->password);
    } else {
        nvs_erase_key(handle, "wifi_ap_pass");  // 빈 password = 삭제
    }
    nvs_set_u8(handle, "wifi_ap_chan", config->channel);
    nvs_set_u8(handle, "wifi_ap_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);

    // 설정 변경 후 전체 데이터 이벤트 발행 (파라미터 값 직접 사용)
    publish_config_event_with_ap(config);

    return ESP_OK;
}

// 내부용 (이벤트 발행 안 함)
esp_err_t ConfigServiceClass::setWiFiAPInternal(const config_wifi_ap_t* config)
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
    // password: 빈 문자열이면 NVS 키 삭제, otherwise 저장
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_ap_pass", config->password);
    } else {
        nvs_erase_key(handle, "wifi_ap_pass");  // 빈 password = 삭제
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
    // password: 빈 문자열이면 NVS 키 삭제, otherwise 저장
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_sta_pass", config->password);
        T_LOGD(TAG, "WiFi STA password save: len=%d", strlen(config->password));
    } else {
        nvs_erase_key(handle, "wifi_sta_pass");  // 빈 password = 삭제
        T_LOGD(TAG, "WiFi STA password erased (empty)");
    }
    nvs_set_u8(handle, "wifi_sta_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);

    // 설정 변경 후 전체 데이터 이벤트 발행 (파라미터 값 직접 사용)
    publish_config_event_with_sta(config);

    return ESP_OK;
}

// 내부용 (이벤트 발행 안 함)
esp_err_t ConfigServiceClass::setWiFiSTAInternal(const config_wifi_sta_t* config)
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
    // password: 빈 문자열이면 NVS 키 삭제, otherwise 저장
    if (config->password[0] != '\0') {
        nvs_set_str(handle, "wifi_sta_pass", config->password);
    } else {
        nvs_erase_key(handle, "wifi_sta_pass");  // 빈 password = 삭제
    }
    nvs_set_u8(handle, "wifi_sta_enbl", config->enabled ? 1 : 0);

    nvs_commit(handle);
    nvs_close(handle);

    return ESP_OK;
}

// ============================================================================
// Ethernet 설정
// ============================================================================

/**
 * @brief IP 주소 형식 검증 (간단한 체크)
 * @return true 유효한 IP 주소 (null이거나 빈 문자열도 허용)
 */
static bool is_valid_ip_string(const char* ip)
{
    if (!ip || ip[0] == '\0') {
        return true;  // 빈 문자열은 유효 (미설정)
    }

    // 간단한 체크: printable ASCII 문자만 허용, '.' 포함
    int dot_count = 0;
    for (int i = 0; ip[i] != '\0' && i < 16; i++) {
        if (ip[i] == '.') {
            dot_count++;
        } else if (ip[i] < '0' || ip[i] > '9') {
            // 숫자와 '.' 외의 문자가 있으면 유효하지 않음
            return false;
        }
    }

    // IP 주소는 최소 3개의 '.'를 가져야 함
    return (dot_count == 3);
}

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

    // static_ip
    size_t len = sizeof(config->static_ip);
    if (nvs_get_str(handle, "eth_static_ip", config->static_ip, &len) != ESP_OK) {
        config->static_ip[0] = '\0';
    } else {
        config->static_ip[sizeof(config->static_ip) - 1] = '\0';
        // 유효성 검사: 쓰레기 값 필터링
        if (!is_valid_ip_string(config->static_ip)) {
            T_LOGW(TAG, "Invalid eth_static_ip in NVS, clearing");
            config->static_ip[0] = '\0';
        }
    }

    // static_netmask
    len = sizeof(config->static_netmask);
    if (nvs_get_str(handle, "eth_static_net", config->static_netmask, &len) != ESP_OK) {
        config->static_netmask[0] = '\0';
    } else {
        config->static_netmask[sizeof(config->static_netmask) - 1] = '\0';
        if (!is_valid_ip_string(config->static_netmask)) {
            T_LOGW(TAG, "Invalid eth_static_netmask in NVS, clearing");
            config->static_netmask[0] = '\0';
        }
    }

    // static_gateway
    len = sizeof(config->static_gateway);
    if (nvs_get_str(handle, "eth_static_gw", config->static_gateway, &len) != ESP_OK) {
        config->static_gateway[0] = '\0';
    } else {
        config->static_gateway[sizeof(config->static_gateway) - 1] = '\0';
        if (!is_valid_ip_string(config->static_gateway)) {
            T_LOGW(TAG, "Invalid eth_static_gateway in NVS, clearing");
            config->static_gateway[0] = '\0';
        }
    }

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

    // 설정 변경 후 전체 데이터 이벤트 발행 (파라미터 값 직접 사용)
    publish_config_event_with_eth(config);

    return ESP_OK;
}

// 내부용 (이벤트 발행 안 함)
esp_err_t ConfigServiceClass::setEthernetInternal(const config_ethernet_t* config)
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
    config->device.rf.frequency = NVS_LORA_DEFAULT_FREQ_868;
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
    config->primary.debug_packet = NVS_SWITCHER_PRI_DEBUG_PACKET;

    // Switcher 기본값 - Secondary
    config->secondary.type = NVS_SWITCHER_SEC_TYPE;
    strncpy(config->secondary.ip, NVS_SWITCHER_SEC_IP, sizeof(config->secondary.ip) - 1);
    config->secondary.port = NVS_SWITCHER_SEC_PORT;
    strncpy(config->secondary.password, NVS_SWITCHER_SEC_PASSWORD, sizeof(config->secondary.password) - 1);
    config->secondary.interface = NVS_SWITCHER_SEC_INTERFACE;
    config->secondary.camera_limit = NVS_SWITCHER_SEC_CAMERA_LIMIT;
    config->secondary.debug_packet = NVS_SWITCHER_SEC_DEBUG_PACKET;

    // Dual 모드 설정
    config->dual_enabled = NVS_DUAL_ENABLED;
    config->secondary_offset = NVS_DUAL_OFFSET;

    T_LOGI(TAG, "defaults loaded");
    return ESP_OK;
}

esp_err_t ConfigServiceClass::factoryReset(void)
{
    T_LOGI(TAG, "factory reset in progress...");

    // NVS 파티션 전체 초기화 (이전 펌웨어 데이터 포함)
    esp_err_t ret = nvs_flash_erase();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS flash erase failed: %s", esp_err_to_name(ret));
        return ret;
    }

    T_LOGI(TAG, "NVS flash erased completely");

    // NVS 재초기화
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        // NVS가 이미 초기화된 경우 (ESP_ERR_NVS_NO_FREE_PAGES 등)
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ret = nvs_flash_erase();
            if (ret == ESP_OK) {
                ret = nvs_flash_init();
            }
        }
        if (ret != ESP_OK) {
            T_LOGE(TAG, "NVS flash re-init failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    T_LOGI(TAG, "NVS flash re-initialized");

    // 기본값 저장
    config_all_t defaultConfig;
    loadDefaults(&defaultConfig);

    return saveAll(&defaultConfig);
}

// ============================================================================
// Device 설정
// ============================================================================

esp_err_t ConfigServiceClass::getDevice(config_device_t* config, int chip_type)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(config_device_t));

    // 칩 타입에 따른 기본 주파수 결정
    float default_freq = NVS_LORA_DEFAULT_FREQ_868;
    if (chip_type == 2) {
        default_freq = NVS_LORA_DEFAULT_FREQ_433;
    }

    // 기본값 설정 (NVSConfig)
    config->brightness = NVS_DEVICE_BRIGHTNESS;
    config->camera_id = NVS_DEVICE_CAMERA_ID;
    config->rf.frequency = default_freq;
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

    // 칩 타입에 따른 주파수 유효성 검증 및 자동 교정
    // SX1268: 433MHz만 지원, SX1262: 868MHz만 지원
    if (config->rf.frequency > 0) {
        bool needs_override = false;
        float correct_freq = 0.0f;

        // 433MHz 대역 범위: 420-450MHz, 868MHz 대역 범위: 850-900MHz
        if (config->rf.frequency >= 420.0f && config->rf.frequency <= 450.0f) {
            // 현재 주파수가 433MHz 대역
            // SX1262(1)는 433MHz를 지원하지 않음 → 868MHz로 교정 필요
            if (chip_type == 1) {
                correct_freq = 868.0f;
                needs_override = true;
            } else {
                // SX1268(2)는 433MHz 지원
                correct_freq = config->rf.frequency;
            }
        } else if (config->rf.frequency >= 850.0f && config->rf.frequency <= 900.0f) {
            // 현재 주파수가 868MHz 대역
            // SX1268(2)은 868MHz를 지원하지 않음 → 433MHz로 교정 필요
            if (chip_type == 2) {
                correct_freq = 433.0f;
                needs_override = true;
            } else {
                // SX1262(1)는 868MHz 지원
                correct_freq = config->rf.frequency;
            }
        } else {
            // 명확하지 않은 주파수인 경우, 칩 타입에 따라 기본값 설정
            if (chip_type == 2) {  // SX1268
                correct_freq = 433.0f;
                needs_override = true;
            } else if (chip_type == 1) {  // SX1262
                correct_freq = 868.0f;
                needs_override = true;
            }
        }

        if (needs_override) {
            T_LOGW(TAG, "칩 타입과 NVS 주파수 불일치 감지");
            T_LOGW(TAG, "  칩 타입: %s (0x%02X)",
                     chip_type == 2 ? "SX1268(433MHz)" : "SX1262(868MHz)",
                     chip_type);
            T_LOGW(TAG, "  NVS 주파수: %.1f MHz", config->rf.frequency);
            T_LOGW(TAG, "  -> 칩 타입에 맞춰 %.1f MHz로 자동 교정합니다", correct_freq);

            config->rf.frequency = correct_freq;

            // 교정된 주파수를 NVS에 즉시 저장
            nvs_set_u32(handle, "dev_frequency", (uint32_t)(correct_freq * 10));
            nvs_commit(handle);
        }
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
    esp_err_t ret = setBrightnessInternal(brightness);
    if (ret != ESP_OK) {
        return ret;
    }

    // 밝기 변경 이벤트 발행 (0-255 범위)
    event_bus_publish(EVT_BRIGHTNESS_CHANGED, &brightness, sizeof(brightness));
    T_LOGD(TAG, "brightness changed: %d, event published", brightness);

    return ESP_OK;
}

// 내부용 (이벤트 발행 안 함)
esp_err_t ConfigServiceClass::setBrightnessInternal(uint8_t brightness)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev, 0);
    if (ret != ESP_OK && ret != 0x105) {
        return ret;
    }

    dev.brightness = brightness;
    return setDevice(&dev);
}

// 내부용 (이벤트 발행 안 함)
esp_err_t ConfigServiceClass::setCameraIdInternal(uint8_t camera_id)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev, 0);
    if (ret != ESP_OK && ret != 0x105) {
        return ret;
    }

    dev.camera_id = camera_id;
    return setDevice(&dev);
}

esp_err_t ConfigServiceClass::setCameraId(uint8_t camera_id)
{
    esp_err_t ret = setCameraIdInternal(camera_id);
    if (ret != ESP_OK) {
        return ret;
    }

    // 카메라 ID 변경 이벤트 발행
    event_bus_publish(EVT_CAMERA_ID_CHANGED, &camera_id, sizeof(camera_id));
    T_LOGD(TAG, "camera_id changed: %d, event published", camera_id);

    return ESP_OK;
}

uint8_t ConfigServiceClass::getCameraId(void)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev, 0);
    if (ret != ESP_OK) {
        return 1;  // 기본값
    }
    return dev.camera_id;
}

esp_err_t ConfigServiceClass::setRf(float frequency, uint8_t sync_word)
{
    config_device_t dev;
    esp_err_t ret = getDevice(&dev, 0);
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
        T_LOGW(TAG, "getPrimary: NVS open failed, using defaults");
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
    T_LOGI(TAG, "setSecondaryOffset: %d", offset);

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

    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief LED 색상 내부 설정 함수 (이벤트 핸들러에서 호출)
 */
esp_err_t ConfigServiceClass::setLedColorsInternal(const config_led_colors_t* config)
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
        T_LOGE(TAG, "registered device count exceeded: %d", devices.count);
        return ESP_ERR_NO_MEM;
    }

    // device_limit 체크
    uint8_t device_limit = license_service_get_device_limit();
    if (device_limit > 0 && devices.count >= device_limit) {
        T_LOGW(TAG, "device_limit exceeded (%d/%d), device register denied: [%02X%02X]",
                 devices.count, device_limit, device_id[0], device_id[1]);
        return ESP_ERR_NO_MEM;
    }

    // NVS 저장
    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS open failed");
        return ESP_FAIL;
    }

    // 디바이스 추가
    memcpy(devices.device_ids[devices.count], device_id, LORA_DEVICE_ID_LEN);
    devices.count++;

    nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, devices.count);

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_PREFIX, devices.count - 1);
    nvs_set_blob(handle, key, devices.device_ids[devices.count - 1], LORA_DEVICE_ID_LEN);

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        char id_str[5];
        device_id_to_str(device_id, id_str);
        T_LOGI(TAG, "device registered: %s (%d/%d)",
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
        T_LOGI(TAG, "device unregistered: %s", id_str);

        // 카메라 매핑도 삭제
        removeDeviceCamMap(device_id);
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
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
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

    // device_limit 초과분 삭제 (라이선스 다운그레이드 대응)
    uint8_t device_limit = license_service_get_device_limit();
    if (device_limit > 0 && devices->count > device_limit) {
        T_LOGW(TAG, "registered devices(%d) exceeds device_limit(%d), deleting excess",
                 devices->count, device_limit);

        // device_limit까지만 유지, 나머지는 NVS에서 삭제
        for (uint8_t i = device_limit; i < devices->count; i++) {
            char key[16];
            snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEVICE_PREFIX, i);
            nvs_erase_key(handle, key);
        }

        // count 업데이트
        devices->count = device_limit;
        nvs_set_u8(handle, NVS_KEY_DEVICE_COUNT, devices->count);
        nvs_commit(handle);

        T_LOGD(TAG, "excess deletion complete, retained devices: %d", devices->count);
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

    T_LOGI(TAG, "all registered devices deleted");
}

// ============================================================================
// 디바이스 카메라 ID 매핑 (TX 장치 기억)
// ============================================================================

// NVS 키: "dev_cam_{index}" - 각 매핑은 3바이트 (device_id[2] + camera_id[1])
#define NVS_KEY_DEV_CAM_PREFIX "dev_cam_"

esp_err_t ConfigServiceClass::setDeviceCameraId(const uint8_t* device_id, uint8_t camera_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS open failed (setDeviceCameraId)");
        return ret;
    }

    // 기존 매핑 확인
    int existing_idx = -1;
    int empty_idx = -1;
    uint8_t current_count = 0;
    for (uint8_t i = 0; i < CONFIG_MAX_DEVICE_CAM_MAP; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);

        uint8_t data[3];
        size_t len = sizeof(data);
        ret = nvs_get_blob(handle, key, data, &len);

        if (ret == ESP_OK) {
            current_count++;
            // device_id 비교
            if (data[0] == device_id[0] && data[1] == device_id[1]) {
                existing_idx = i;
                break;
            }
        } else if (ret == ESP_ERR_NVS_NOT_FOUND && empty_idx < 0) {
            empty_idx = i;
        }
    }

    // 기존 매핑 업데이트 또는 새 매핑 추가
    int target_idx = (existing_idx >= 0) ? existing_idx : empty_idx;

    // 신규 매핑인 경우 device_limit 체크
    if (existing_idx < 0) {
        uint8_t device_limit = license_service_get_device_limit();
        if (current_count >= device_limit) {
            nvs_close(handle);
            T_LOGW(TAG, "device_limit exceeded (%d/%d), mapping denied: [%02X%02X]",
                     current_count, device_limit, device_id[0], device_id[1]);
            return ESP_ERR_NO_MEM;
        }
    }

    // NVS 꽉 찼을 때
    if (target_idx < 0) {
        nvs_close(handle);
        T_LOGW(TAG, "device-camera map full");
        return ESP_ERR_NO_MEM;
    }

    uint8_t data[3] = {device_id[0], device_id[1], camera_id};
    char key[32];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, target_idx);

    ret = nvs_set_blob(handle, key, data, sizeof(data));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
        if (ret == ESP_OK) {
            T_LOGD(TAG, "device-camera map saved: [%02X%02X] → Cam%d (idx=%d)", device_id[0], device_id[1], camera_id, target_idx);

            // 캐시 업데이트
            bool found = false;
            for (uint8_t i = 0; i < s_device_cam_map.count; i++) {
                if (s_device_cam_map.device_ids[i][0] == device_id[0] &&
                    s_device_cam_map.device_ids[i][1] == device_id[1]) {
                    s_device_cam_map.camera_ids[i] = camera_id;
                    found = true;
                    break;
                }
            }
            if (!found && s_device_cam_map.count < CONFIG_MAX_DEVICE_CAM_MAP) {
                s_device_cam_map.device_ids[s_device_cam_map.count][0] = device_id[0];
                s_device_cam_map.device_ids[s_device_cam_map.count][1] = device_id[1];
                s_device_cam_map.camera_ids[s_device_cam_map.count] = camera_id;
                s_device_cam_map.count++;
            }
        }
    }

    nvs_close(handle);
    return ret;
}

esp_err_t ConfigServiceClass::getDeviceCameraId(const uint8_t* device_id, uint8_t* camera_id)
{
    if (!device_id || !camera_id) {
        return ESP_ERR_INVALID_ARG;
    }

    // 캐시 확인
    for (uint8_t i = 0; i < s_device_cam_map.count; i++) {
        if (s_device_cam_map.device_ids[i][0] == device_id[0] &&
            s_device_cam_map.device_ids[i][1] == device_id[1]) {
            *camera_id = s_device_cam_map.camera_ids[i];
            return ESP_OK;
        }
    }

    // NVS 확인 (캐시 미스 시)
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    for (uint8_t i = 0; i < CONFIG_MAX_DEVICE_CAM_MAP; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);

        uint8_t data[3];
        size_t len = sizeof(data);
        ret = nvs_get_blob(handle, key, data, &len);

        if (ret == ESP_OK) {
            if (data[0] == device_id[0] && data[1] == device_id[1]) {
                *camera_id = data[2];
                nvs_close(handle);

                // 캐시에 추가
                if (s_device_cam_map.count < CONFIG_MAX_DEVICE_CAM_MAP) {
                    s_device_cam_map.device_ids[s_device_cam_map.count][0] = device_id[0];
                    s_device_cam_map.device_ids[s_device_cam_map.count][1] = device_id[1];
                    s_device_cam_map.camera_ids[s_device_cam_map.count] = *camera_id;
                    s_device_cam_map.count++;
                }

                return ESP_OK;
            }
        }
    }

    nvs_close(handle);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ConfigServiceClass::getDeviceCamMap(config_device_cam_map_t* map)
{
    if (!map) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        // NVS 없으면 캐시 반환
        *map = s_device_cam_map;
        return ESP_OK;
    }

    map->count = 0;
    for (uint8_t i = 0; i < CONFIG_MAX_DEVICE_CAM_MAP; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);

        uint8_t data[3];
        size_t len = sizeof(data);
        ret = nvs_get_blob(handle, key, data, &len);

        if (ret == ESP_OK) {
            map->device_ids[map->count][0] = data[0];
            map->device_ids[map->count][1] = data[1];
            map->camera_ids[map->count] = data[2];
            map->count++;
        }
    }

    // device_limit 초과분 삭제 (라이선스 다운그레이드 대응)
    uint8_t device_limit = license_service_get_device_limit();
    if (map->count > device_limit) {
        T_LOGW(TAG, "device-camera map(%d) exceeds device_limit(%d), deleting excess",
                 map->count, device_limit);

        // device_limit까지만 유지, 나머지는 NVS에서 삭제
        for (uint8_t i = device_limit; i < map->count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);
            nvs_erase_key(handle, key);
        }

        map->count = device_limit;
        nvs_commit(handle);

        T_LOGI(TAG, "excess deletion complete, retained mappings: %d", map->count);
    }

    nvs_close(handle);

    // 캐시 업데이트
    s_device_cam_map = *map;

    return ESP_OK;
}

esp_err_t ConfigServiceClass::removeDeviceCamMap(const uint8_t* device_id)
{
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    for (uint8_t i = 0; i < CONFIG_MAX_DEVICE_CAM_MAP; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);

        uint8_t data[3];
        size_t len = sizeof(data);
        ret = nvs_get_blob(handle, key, data, &len);

        if (ret == ESP_OK) {
            if (data[0] == device_id[0] && data[1] == device_id[1]) {
                nvs_erase_key(handle, key);
                nvs_commit(handle);

                T_LOGI(TAG, "device-camera map deleted: [%02X%02X]", device_id[0], device_id[1]);

                // 캐시에서 제거
                for (uint8_t j = i; j < s_device_cam_map.count - 1; j++) {
                    s_device_cam_map.device_ids[j][0] = s_device_cam_map.device_ids[j + 1][0];
                    s_device_cam_map.device_ids[j][1] = s_device_cam_map.device_ids[j + 1][1];
                    s_device_cam_map.camera_ids[j] = s_device_cam_map.camera_ids[j + 1];
                }
                if (s_device_cam_map.count > 0) {
                    s_device_cam_map.count--;
                }

                nvs_close(handle);
                return ESP_OK;
            }
        }
    }

    nvs_close(handle);
    return ESP_ERR_NOT_FOUND;
}

void ConfigServiceClass::clearDeviceCamMap(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE_DEVICES, NVS_READWRITE, &handle) == ESP_OK) {
        for (uint8_t i = 0; i < CONFIG_MAX_DEVICE_CAM_MAP; i++) {
            char key[32];
            snprintf(key, sizeof(key), "%s%d", NVS_KEY_DEV_CAM_PREFIX, i);
            nvs_erase_key(handle, key);
        }
        nvs_commit(handle);
        nvs_close(handle);
    }

    s_device_cam_map.count = 0;
    T_LOGI(TAG, "all device-camera mappings deleted");
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t config_service_init(void)
{
    return ConfigServiceClass::init();
}

esp_err_t config_service_apply_device_limit(void)
{
    return ConfigServiceClass::applyDeviceLimit();
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

esp_err_t config_service_get_device(config_device_t* config, int chip_type)
{
    return ConfigServiceClass::getDevice(config, chip_type);
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

// ============================================================================
// 디바이스 카메라 ID 매핑 (TX 장치 기억)
// ============================================================================

esp_err_t config_service_set_device_camera_id(const uint8_t* device_id, uint8_t camera_id)
{
    return ConfigServiceClass::setDeviceCameraId(device_id, camera_id);
}

esp_err_t config_service_get_device_camera_id(const uint8_t* device_id, uint8_t* camera_id)
{
    return ConfigServiceClass::getDeviceCameraId(device_id, camera_id);
}

esp_err_t config_service_get_device_cam_map(config_device_cam_map_t* map)
{
    return ConfigServiceClass::getDeviceCamMap(map);
}

esp_err_t config_service_remove_device_cam_map(const uint8_t* device_id)
{
    return ConfigServiceClass::removeDeviceCamMap(device_id);
}

void config_service_clear_device_cam_map(void)
{
    ConfigServiceClass::clearDeviceCamMap();
}

// ============================================================================
// 라이센스 데이터 (NVS "license" 네임스페이스)
// ============================================================================

/**
 * @brief NVS에서 라이센스 데이터 읽기
 */
esp_err_t ConfigServiceClass::getLicenseData(uint8_t* device_limit, char* key)
{
    if (!device_limit || !key) {
        return ESP_ERR_INVALID_ARG;
    }

    *device_limit = 0;
    key[0] = '\0';

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("license", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // device_limit 읽기
    uint8_t limit = 0;
    nvs_get_u8(handle, "device_limit", &limit);
    *device_limit = limit;

    // key 읽기
    size_t key_len = 17;
    nvs_get_str(handle, "license_key", key, &key_len);

    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief NVS에 라이센스 데이터 저장
 */
esp_err_t ConfigServiceClass::setLicenseData(uint8_t device_limit, const char* key)
{
    license_data_event_t data;
    data.device_limit = device_limit;
    if (key) {
        strncpy(data.key, key, 16);
        data.key[16] = '\0';
    } else {
        data.key[0] = '\0';
    }
    return setLicenseDataInternal(&data);
}

/**
 * @brief NVS에 라이센스 데이터 저장 (내부 함수)
 */
esp_err_t ConfigServiceClass::setLicenseDataInternal(const license_data_event_t* data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("license", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_u8(handle, "device_limit", data->device_limit);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, "license_key", data->key);
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

} // extern "C"
