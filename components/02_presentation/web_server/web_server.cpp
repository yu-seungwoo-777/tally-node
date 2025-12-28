/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API (Event-based)
 */

#include "web_server.h"
#include "event_bus.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include <cstring>
#include <cmath>

static const char* TAG = "WebServer";
static httpd_handle_t s_server = nullptr;

// ============================================================================
// 정적 파일 (임베디드) - web/ 폴더에서 build 시 생성됨
// ============================================================================
#include "static_files.h"

// ============================================================================
// 내부 데이터 캐시 (event_bus 구조체 그대로 사용)
// ============================================================================

typedef struct {
    system_info_event_t system;       // EVT_INFO_UPDATED
    bool system_valid;

    switcher_status_event_t switcher; // EVT_SWITCHER_STATUS_CHANGED
    bool switcher_valid;

    network_status_event_t network;   // EVT_NETWORK_STATUS_CHANGED
    bool network_valid;

    config_data_event_t config;       // EVT_CONFIG_DATA_CHANGED
    bool config_valid;
} web_server_data_t;

static web_server_data_t s_cache;

// 초기화를 위한 정적 함수
static void init_cache(void)
{
    memset(&s_cache, 0, sizeof(s_cache));
    s_cache.system_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.config_valid = false;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 시스템 정보 이벤트 핸들러 (EVT_INFO_UPDATED)
 */
static esp_err_t onSystemInfoEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const system_info_event_t* info = (const system_info_event_t*)event->data;
    memcpy(&s_cache.system, info, sizeof(system_info_event_t));
    s_cache.system_valid = true;

    return ESP_OK;
}

/**
 * @brief 스위처 상태 이벤트 핸들러 (EVT_SWITCHER_STATUS_CHANGED)
 */
static esp_err_t onSwitcherStatusEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const switcher_status_event_t* status = (const switcher_status_event_t*)event->data;
    memcpy(&s_cache.switcher, status, sizeof(switcher_status_event_t));
    s_cache.switcher_valid = true;

    return ESP_OK;
}

/**
 * @brief 네트워크 상태 이벤트 핸들러 (EVT_NETWORK_STATUS_CHANGED)
 */
static esp_err_t onNetworkStatusEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const network_status_event_t* status = (const network_status_event_t*)event->data;
    memcpy(&s_cache.network, status, sizeof(network_status_event_t));
    s_cache.network_valid = true;

    return ESP_OK;
}

/**
 * @brief 설정 데이터 이벤트 핸들러 (EVT_CONFIG_DATA_CHANGED)
 */
static esp_err_t onConfigDataEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    // DisplayManager 패턴: 구조체 그대로 복사
    const config_data_event_t* config = (const config_data_event_t*)event->data;
    s_cache.config = *config;
    s_cache.config_valid = true;

    return ESP_OK;
}

// ============================================================================
// Packed 데이터 → PGM/PVW 리스트 변환
// ============================================================================

/**
 * @brief packed 데이터에서 채널 상태 가져오기
 * @param data packed 데이터
 * @param channel 채널 번호 (1-20)
 * @return 0=off, 1=pgm, 2=pvw, 3=both
 */
static uint8_t get_channel_state(const uint8_t* data, uint8_t channel)
{
    if (!data || channel < 1 || channel > 20) {
        return 0;
    }
    uint8_t byte_idx = (channel - 1) / 4;
    uint8_t bit_idx = ((channel - 1) % 4) * 2;
    return (data[byte_idx] >> bit_idx) & 0x03;
}

/**
 * @brief packed 데이터를 hex 문자열로 변환
 */
static void packed_to_hex(const uint8_t* data, uint8_t size, char* out, size_t out_size)
{
    if (!data || !out || out_size < (size * 2 + 1)) {
        if (out) out[0] = '\0';
        return;
    }
    for (uint8_t i = 0; i < size; i++) {
        snprintf(out + (i * 2), 3, "%02X", data[i]);
    }
}

// ============================================================================
// API 핸들러
// ============================================================================

/**
 * @brief GET /api/status - 전체 상태 반환 (캐시 데이터 사용)
 */
static esp_err_t api_status_handler(httpd_req_t* req)
{
    cJSON* root = cJSON_CreateObject();

    // Network (상태 + 설정 통합)
    cJSON* network = cJSON_CreateObject();

    // AP (상태 + 설정)
    cJSON* ap = cJSON_CreateObject();
    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(ap, "enabled", s_cache.config.wifi_ap_enabled);
        cJSON_AddStringToObject(ap, "ssid", s_cache.config.wifi_ap_ssid);
        cJSON_AddNumberToObject(ap, "channel", s_cache.config.wifi_ap_channel);
        // AP IP는 상태에서 (시작 후 할당됨)
        if (s_cache.network_valid) {
            cJSON_AddStringToObject(ap, "ip", (s_cache.config.wifi_ap_enabled && s_cache.network.ap_ip[0] != '\0') ? s_cache.network.ap_ip : "--");
        } else {
            cJSON_AddStringToObject(ap, "ip", "--");
        }
    } else {
        cJSON_AddBoolToObject(ap, "enabled", false);
        cJSON_AddStringToObject(ap, "ssid", "--");
        cJSON_AddNumberToObject(ap, "channel", 1);
        cJSON_AddStringToObject(ap, "ip", "--");
    }
    cJSON_AddItemToObject(network, "ap", ap);

    // WiFi STA (상태 + 설정)
    cJSON* wifi = cJSON_CreateObject();
    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(wifi, "enabled", s_cache.config.wifi_sta_enabled);
        cJSON_AddStringToObject(wifi, "ssid", s_cache.config.wifi_sta_ssid);
    } else {
        cJSON_AddBoolToObject(wifi, "enabled", false);
        cJSON_AddStringToObject(wifi, "ssid", "--");
    }
    // 연결 상태는 network 이벤트에서
    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(wifi, "connected", s_cache.network.sta_connected);
        cJSON_AddStringToObject(wifi, "ip", s_cache.network.sta_connected ? s_cache.network.sta_ip : "--");
    } else {
        cJSON_AddBoolToObject(wifi, "connected", false);
        cJSON_AddStringToObject(wifi, "ip", "--");
    }
    cJSON_AddItemToObject(network, "wifi", wifi);

    // Ethernet (상태 + 설정)
    cJSON* ethernet = cJSON_CreateObject();
    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(ethernet, "enabled", s_cache.config.eth_enabled);
        cJSON_AddBoolToObject(ethernet, "dhcp", s_cache.config.eth_dhcp_enabled);
        cJSON_AddStringToObject(ethernet, "staticIp", s_cache.config.eth_static_ip);
        cJSON_AddStringToObject(ethernet, "netmask", s_cache.config.eth_static_netmask);
        cJSON_AddStringToObject(ethernet, "gateway", s_cache.config.eth_static_gateway);
    } else {
        cJSON_AddBoolToObject(ethernet, "enabled", false);
        cJSON_AddBoolToObject(ethernet, "dhcp", true);
        cJSON_AddStringToObject(ethernet, "staticIp", "");
        cJSON_AddStringToObject(ethernet, "netmask", "");
        cJSON_AddStringToObject(ethernet, "gateway", "");
    }
    // 연결 상태와 IP는 network 이벤트에서
    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(ethernet, "connected", s_cache.network.eth_connected);
        cJSON_AddStringToObject(ethernet, "ip", s_cache.network.eth_connected ? s_cache.network.eth_ip : "--");
    } else {
        cJSON_AddBoolToObject(ethernet, "connected", false);
        cJSON_AddStringToObject(ethernet, "ip", "--");
    }
    cJSON_AddItemToObject(network, "ethernet", ethernet);

    cJSON_AddItemToObject(root, "network", network);

    // Switcher (EVT_SWITCHER_STATUS_CHANGED)
    cJSON* switcher = cJSON_CreateObject();

    // Primary (상태 + 설정 병합)
    cJSON* primary = cJSON_CreateObject();
    if (s_cache.switcher_valid) {
        cJSON_AddBoolToObject(primary, "connected", s_cache.switcher.s1_connected);
        cJSON_AddStringToObject(primary, "type", s_cache.switcher.s1_type);
        cJSON_AddStringToObject(primary, "ip", s_cache.switcher.s1_ip);
        cJSON_AddNumberToObject(primary, "port", s_cache.switcher.s1_port);
        // 설정 (config에서)
        if (s_cache.config_valid) {
            cJSON_AddNumberToObject(primary, "interface", s_cache.config.primary_interface);
            cJSON_AddNumberToObject(primary, "cameraLimit", s_cache.config.primary_camera_limit);
        } else {
            cJSON_AddNumberToObject(primary, "interface", 2);
            cJSON_AddNumberToObject(primary, "cameraLimit", 0);
        }

        // Tally Primary
        cJSON* p_tally = cJSON_CreateObject();
        cJSON* p_pgm = cJSON_CreateArray();
        cJSON* p_pvw = cJSON_CreateArray();
        uint8_t p_count = s_cache.switcher.s1_channel_count;
        for (uint8_t i = 0; i < p_count && i < 20; i++) {
            uint8_t state = get_channel_state(s_cache.switcher.s1_tally_data, i + 1);
            if (state == 1 || state == 3) {  // pgm or both
                cJSON_AddItemToArray(p_pgm, cJSON_CreateNumber(i + 1));
            }
            if (state == 2 || state == 3) {  // pvw or both
                cJSON_AddItemToArray(p_pvw, cJSON_CreateNumber(i + 1));
            }
        }
        cJSON_AddItemToObject(p_tally, "pgm", p_pgm);
        cJSON_AddItemToObject(p_tally, "pvw", p_pvw);

        // Raw hex (JS 해석용)
        char p_hex[17] = {0};
        uint8_t p_bytes = (p_count + 3) / 4;
        packed_to_hex(s_cache.switcher.s1_tally_data, p_bytes, p_hex, sizeof(p_hex));
        cJSON_AddStringToObject(p_tally, "raw", p_hex);
        cJSON_AddNumberToObject(p_tally, "channels", p_count);

        cJSON_AddItemToObject(primary, "tally", p_tally);
    } else {
        cJSON_AddBoolToObject(primary, "connected", false);
        cJSON_AddStringToObject(primary, "type", "--");
        cJSON_AddStringToObject(primary, "ip", "--");
        cJSON_AddNumberToObject(primary, "port", 0);
        cJSON_AddNumberToObject(primary, "interface", 2);
        cJSON_AddNumberToObject(primary, "cameraLimit", 0);

        cJSON* p_tally = cJSON_CreateObject();
        cJSON_AddItemToObject(p_tally, "pgm", cJSON_CreateArray());
        cJSON_AddItemToObject(p_tally, "pvw", cJSON_CreateArray());
        cJSON_AddStringToObject(p_tally, "raw", "");
        cJSON_AddNumberToObject(p_tally, "channels", 0);
        cJSON_AddItemToObject(primary, "tally", p_tally);
    }
    cJSON_AddItemToObject(switcher, "primary", primary);

    // Secondary (상태 + 설정 병합)
    cJSON* secondary = cJSON_CreateObject();
    if (s_cache.switcher_valid) {
        cJSON_AddBoolToObject(secondary, "connected", s_cache.switcher.s2_connected);
        cJSON_AddStringToObject(secondary, "type", s_cache.switcher.s2_type);
        cJSON_AddStringToObject(secondary, "ip", s_cache.switcher.s2_ip);
        cJSON_AddNumberToObject(secondary, "port", s_cache.switcher.s2_port);
        // 설정 (config에서)
        if (s_cache.config_valid) {
            cJSON_AddNumberToObject(secondary, "interface", s_cache.config.secondary_interface);
            cJSON_AddNumberToObject(secondary, "cameraLimit", s_cache.config.secondary_camera_limit);
        } else {
            cJSON_AddNumberToObject(secondary, "interface", 1);
            cJSON_AddNumberToObject(secondary, "cameraLimit", 0);
        }

        // Tally Secondary
        cJSON* s_tally = cJSON_CreateObject();
        cJSON* s_pgm = cJSON_CreateArray();
        cJSON* s_pvw = cJSON_CreateArray();
        uint8_t s_count = s_cache.switcher.s2_channel_count;
        for (uint8_t i = 0; i < s_count && i < 20; i++) {
            uint8_t state = get_channel_state(s_cache.switcher.s2_tally_data, i + 1);
            if (state == 1 || state == 3) {  // pgm or both
                cJSON_AddItemToArray(s_pgm, cJSON_CreateNumber(i + 1));
            }
            if (state == 2 || state == 3) {  // pvw or both
                cJSON_AddItemToArray(s_pvw, cJSON_CreateNumber(i + 1));
            }
        }
        cJSON_AddItemToObject(s_tally, "pgm", s_pgm);
        cJSON_AddItemToObject(s_tally, "pvw", s_pvw);

        // Raw hex (JS 해석용)
        char s_hex[17] = {0};
        uint8_t s_bytes = (s_count + 3) / 4;
        packed_to_hex(s_cache.switcher.s2_tally_data, s_bytes, s_hex, sizeof(s_hex));
        cJSON_AddStringToObject(s_tally, "raw", s_hex);
        cJSON_AddNumberToObject(s_tally, "channels", s_count);

        cJSON_AddItemToObject(secondary, "tally", s_tally);
    } else {
        cJSON_AddBoolToObject(secondary, "connected", false);
        cJSON_AddStringToObject(secondary, "type", "--");
        cJSON_AddStringToObject(secondary, "ip", "--");
        cJSON_AddNumberToObject(secondary, "port", 0);
        cJSON_AddNumberToObject(secondary, "interface", 1);
        cJSON_AddNumberToObject(secondary, "cameraLimit", 0);

        cJSON* s_tally = cJSON_CreateObject();
        cJSON_AddItemToObject(s_tally, "pgm", cJSON_CreateArray());
        cJSON_AddItemToObject(s_tally, "pvw", cJSON_CreateArray());
        cJSON_AddStringToObject(s_tally, "raw", "");
        cJSON_AddNumberToObject(s_tally, "channels", 0);
        cJSON_AddItemToObject(secondary, "tally", s_tally);
    }
    cJSON_AddItemToObject(switcher, "secondary", secondary);

    // 공통 필드
    cJSON_AddBoolToObject(switcher, "dualEnabled", s_cache.switcher_valid ? s_cache.switcher.dual_mode : false);
    if (s_cache.config_valid) {
        cJSON_AddNumberToObject(switcher, "secondaryOffset", s_cache.config.secondary_offset);
    } else {
        cJSON_AddNumberToObject(switcher, "secondaryOffset", 4);
    }

    cJSON_AddItemToObject(root, "switcher", switcher);

    // System (EVT_INFO_UPDATED)
    cJSON* system = cJSON_CreateObject();
    if (s_cache.system_valid) {
        cJSON_AddStringToObject(system, "deviceId", s_cache.system.device_id);
        cJSON_AddNumberToObject(system, "battery", s_cache.system.battery);
        // 소수점 한 자리로 제한 (4.2, 52.8)
        cJSON_AddNumberToObject(system, "voltage", (round(s_cache.system.voltage * 10) / 10));
        cJSON_AddNumberToObject(system, "temperature", (round(s_cache.system.temperature * 10) / 10));
        cJSON_AddNumberToObject(system, "uptime", s_cache.system.uptime);
    } else {
        cJSON_AddStringToObject(system, "deviceId", "0000");
        cJSON_AddNumberToObject(system, "battery", 0);
        cJSON_AddNumberToObject(system, "voltage", 0);
        cJSON_AddNumberToObject(system, "temperature", 0);
        cJSON_AddNumberToObject(system, "uptime", 0);
    }
    cJSON_AddItemToObject(root, "system", system);

    // Device 설정 (config에서)
    cJSON* device = cJSON_CreateObject();
    if (s_cache.config_valid) {
        cJSON_AddNumberToObject(device, "brightness", s_cache.config.device_brightness);
        cJSON_AddNumberToObject(device, "cameraId", s_cache.config.device_camera_id);

        cJSON* rf = cJSON_CreateObject();
        cJSON_AddNumberToObject(rf, "frequency", s_cache.config.device_rf_frequency);
        cJSON_AddNumberToObject(rf, "syncWord", s_cache.config.device_rf_sync_word);
        cJSON_AddNumberToObject(rf, "spreadingFactor", s_cache.config.device_rf_sf);
        cJSON_AddNumberToObject(rf, "codingRate", s_cache.config.device_rf_cr);
        cJSON_AddNumberToObject(rf, "bandwidth", s_cache.config.device_rf_bw);
        cJSON_AddNumberToObject(rf, "txPower", s_cache.config.device_rf_tx_power);
        cJSON_AddItemToObject(device, "rf", rf);
    } else {
        cJSON_AddNumberToObject(device, "brightness", 128);
        cJSON_AddNumberToObject(device, "cameraId", 1);

        cJSON* rf = cJSON_CreateObject();
        cJSON_AddNumberToObject(rf, "frequency", 868);
        cJSON_AddNumberToObject(rf, "syncWord", 0x12);
        cJSON_AddNumberToObject(rf, "spreadingFactor", 7);
        cJSON_AddNumberToObject(rf, "codingRate", 7);
        cJSON_AddNumberToObject(rf, "bandwidth", 250);
        cJSON_AddNumberToObject(rf, "txPower", 22);
        cJSON_AddItemToObject(device, "rf", rf);
    }
    cJSON_AddItemToObject(root, "device", device);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief POST /api/config/path - 설정 저장 (이벤트 기반)
 */
static esp_err_t api_config_post_handler(httpd_req_t* req)
{
    const char* uri = req->uri;
    const char* prefix = "/api/config/";
    size_t prefix_len = strlen(prefix);

    if (strncmp(uri, prefix, prefix_len) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI");
        return ESP_FAIL;
    }

    const char* path = uri + prefix_len;

    // 요청 바디 읽기
    char* buf = new char[512];
    int ret = httpd_req_recv(req, buf, 511);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;

    if (root == nullptr) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 설정 저장 요청 이벤트 데이터
    config_save_request_t save_req = {};

    // 경로별 처리
    if (strncmp(path, "device/brightness", 16) == 0) {
        cJSON* item = cJSON_GetObjectItem(root, "value");
        if (item && cJSON_IsNumber(item)) {
            save_req.type = CONFIG_SAVE_DEVICE_BRIGHTNESS;
            save_req.brightness = (uint8_t)item->valueint;
        } else {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'value'");
            return ESP_FAIL;
        }
    }
    else if (strncmp(path, "device/camera_id", 15) == 0) {
        cJSON* item = cJSON_GetObjectItem(root, "value");
        if (item && cJSON_IsNumber(item)) {
            save_req.type = CONFIG_SAVE_DEVICE_CAMERA_ID;
            save_req.camera_id = (uint8_t)item->valueint;
        } else {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid 'value'");
            return ESP_FAIL;
        }
    }
    else if (strncmp(path, "device/rf", 9) == 0) {
        cJSON* freq = cJSON_GetObjectItem(root, "frequency");
        cJSON* sync = cJSON_GetObjectItem(root, "syncWord");
        if (freq && sync && cJSON_IsNumber(freq) && cJSON_IsNumber(sync)) {
            save_req.type = CONFIG_SAVE_DEVICE_RF;
            save_req.rf_frequency = (float)freq->valuedouble;
            save_req.rf_sync_word = (uint8_t)sync->valueint;
        } else {
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'frequency' or 'syncWord'");
            return ESP_FAIL;
        }
    }
    else if (strncmp(path, "switcher/primary", 16) == 0) {
        cJSON* type = cJSON_GetObjectItem(root, "type");
        cJSON* ip = cJSON_GetObjectItem(root, "ip");
        cJSON* port = cJSON_GetObjectItem(root, "port");

        save_req.type = CONFIG_SAVE_SWITCHER_PRIMARY;
        if (type && type->valuestring) {
            strncpy(save_req.switcher_type, type->valuestring, sizeof(save_req.switcher_type) - 1);
        }
        if (ip && cJSON_IsString(ip)) {
            strncpy(save_req.switcher_ip, ip->valuestring, sizeof(save_req.switcher_ip) - 1);
        }
        if (port && cJSON_IsNumber(port)) {
            save_req.switcher_port = (uint16_t)port->valueint;
        }
    }
    else if (strncmp(path, "switcher/secondary", 18) == 0) {
        cJSON* type = cJSON_GetObjectItem(root, "type");
        cJSON* ip = cJSON_GetObjectItem(root, "ip");
        cJSON* port = cJSON_GetObjectItem(root, "port");

        save_req.type = CONFIG_SAVE_SWITCHER_SECONDARY;
        if (type && type->valuestring) {
            strncpy(save_req.switcher_type, type->valuestring, sizeof(save_req.switcher_type) - 1);
        }
        if (ip && cJSON_IsString(ip)) {
            strncpy(save_req.switcher_ip, ip->valuestring, sizeof(save_req.switcher_ip) - 1);
        }
        if (port && cJSON_IsNumber(port)) {
            save_req.switcher_port = (uint16_t)port->valueint;
        }
    }
    else if (strncmp(path, "switcher/dual", 13) == 0) {
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
        cJSON* offset = cJSON_GetObjectItem(root, "offset");

        save_req.type = CONFIG_SAVE_SWITCHER_DUAL;
        if (enabled && cJSON_IsBool(enabled)) {
            save_req.switcher_dual_enabled = cJSON_IsTrue(enabled);
        }
        if (offset && cJSON_IsNumber(offset)) {
            save_req.switcher_secondary_offset = (uint8_t)offset->valueint;
        }
    }
    else if (strncmp(path, "network/ap", 11) == 0) {
        cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON* password = cJSON_GetObjectItem(root, "password");
        cJSON* channel = cJSON_GetObjectItem(root, "channel");
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

        save_req.type = CONFIG_SAVE_WIFI_AP;
        if (ssid && cJSON_IsString(ssid)) {
            strncpy(save_req.wifi_ap_ssid, ssid->valuestring, sizeof(save_req.wifi_ap_ssid) - 1);
        }
        if (password && cJSON_IsString(password)) {
            strncpy(save_req.wifi_ap_password, password->valuestring, sizeof(save_req.wifi_ap_password) - 1);
        }
        if (channel && cJSON_IsNumber(channel)) {
            save_req.wifi_ap_channel = (uint8_t)channel->valueint;
        }
        if (enabled && cJSON_IsBool(enabled)) {
            save_req.wifi_ap_enabled = cJSON_IsTrue(enabled);
        }

        ESP_LOGI(TAG, "Publishing AP save event: ssid=%s, ch=%d, en=%d",
                 save_req.wifi_ap_ssid, save_req.wifi_ap_channel, save_req.wifi_ap_enabled);
    }
    else if (strncmp(path, "network/wifi", 13) == 0) {
        cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON* password = cJSON_GetObjectItem(root, "password");
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

        save_req.type = CONFIG_SAVE_WIFI_STA;
        if (ssid && cJSON_IsString(ssid)) {
            strncpy(save_req.wifi_sta_ssid, ssid->valuestring, sizeof(save_req.wifi_sta_ssid) - 1);
        }
        if (password && cJSON_IsString(password)) {
            strncpy(save_req.wifi_sta_password, password->valuestring, sizeof(save_req.wifi_sta_password) - 1);
        }
        if (enabled && cJSON_IsBool(enabled)) {
            save_req.wifi_sta_enabled = cJSON_IsTrue(enabled);
        }

        ESP_LOGI(TAG, "Publishing STA save event: ssid=%s, en=%d",
                 save_req.wifi_sta_ssid, save_req.wifi_sta_enabled);
    }
    else if (strncmp(path, "network/ethernet", 16) == 0) {
        cJSON* ip = cJSON_GetObjectItem(root, "staticIp");
        cJSON* gateway = cJSON_GetObjectItem(root, "gateway");
        cJSON* netmask = cJSON_GetObjectItem(root, "netmask");
        cJSON* dhcp = cJSON_GetObjectItem(root, "dhcp");
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

        save_req.type = CONFIG_SAVE_ETHERNET;
        if (dhcp && cJSON_IsBool(dhcp)) {
            save_req.eth_dhcp = cJSON_IsTrue(dhcp);
        }
        if (ip && cJSON_IsString(ip)) {
            strncpy(save_req.eth_static_ip, ip->valuestring, sizeof(save_req.eth_static_ip) - 1);
        }
        if (gateway && cJSON_IsString(gateway)) {
            strncpy(save_req.eth_gateway, gateway->valuestring, sizeof(save_req.eth_gateway) - 1);
        }
        if (netmask && cJSON_IsString(netmask)) {
            strncpy(save_req.eth_netmask, netmask->valuestring, sizeof(save_req.eth_netmask) - 1);
        }
        if (enabled && cJSON_IsBool(enabled)) {
            save_req.eth_enabled = cJSON_IsTrue(enabled);
        }

        ESP_LOGI(TAG, "Publishing Ethernet save event: dhcp=%d, en=%d",
                 save_req.eth_dhcp, save_req.eth_enabled);
    }
    else {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown config path");
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    // 설정 저장 이벤트 발행 (config_service가 NVS 저장 후 EVT_CONFIG_DATA_CHANGED 발행)
    event_bus_publish(EVT_CONFIG_CHANGED, &save_req, sizeof(save_req));

    // EVT_CONFIG_DATA_CHANGED 이벤트가 network_service에 전달될 때까지 대기
    vTaskDelay(pdMS_TO_TICKS(100));

    // 네트워크 설정인 경우 재시작 이벤트도 발행
    if (save_req.type == CONFIG_SAVE_WIFI_AP) {
        network_restart_request_t restart_req = { .type = NETWORK_RESTART_WIFI_AP, .ssid = "", .password = "" };
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    }
    else if (save_req.type == CONFIG_SAVE_WIFI_STA) {
        if (save_req.wifi_sta_enabled) {
            network_restart_request_t restart_req = {
                .type = NETWORK_RESTART_WIFI_STA,
                .ssid = "",
                .password = ""
            };
            strncpy(restart_req.ssid, save_req.wifi_sta_ssid, sizeof(restart_req.ssid) - 1);
            strncpy(restart_req.password, save_req.wifi_sta_password, sizeof(restart_req.password) - 1);
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        } else {
            network_restart_request_t restart_req = { .type = NETWORK_RESTART_WIFI_AP, .ssid = "", .password = "" };
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        }
    }
    else if (save_req.type == CONFIG_SAVE_ETHERNET) {
        network_restart_request_t restart_req = { .type = NETWORK_RESTART_ETHERNET, .ssid = "", .password = "" };
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    }

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/reboot - 시스템 재부팅
 */
static esp_err_t api_reboot_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

/**
 * @brief index.html 핸들러
 */
static esp_err_t index_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_data, index_html_len);
    return ESP_OK;
}

/**
 * @brief CSS 파일 핸들러
 */
static esp_err_t css_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)styles_css_data, styles_css_len);
    return ESP_OK;
}

/**
 * @brief JS 파일 핸들러
 */
static esp_err_t js_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)app_js_data, app_js_len);
    return ESP_OK;
}

// ============================================================================
// URI 등록
// ============================================================================

static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_status = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = api_status_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_POST,
    .handler = api_reboot_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_ap = {
    .uri = "/api/config/network/ap",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_wifi = {
    .uri = "/api/config/network/wifi",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_ethernet = {
    .uri = "/api/config/network/ethernet",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

// 정적 파일 URI
static const httpd_uri_t uri_css = {
    .uri = "/css/styles.css",
    .method = HTTP_GET,
    .handler = css_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_js = {
    .uri = "/js/app.js",
    .method = HTTP_GET,
    .handler = js_handler,
    .user_ctx = nullptr
};

// ============================================================================
// C 인터페이스
// ============================================================================

extern "C" {

esp_err_t web_server_init(void)
{
    // 캐시 초기화
    init_cache();

    if (s_server != nullptr) {
        ESP_LOGW(TAG, "Web server already initialized");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 5;
    config.max_uri_handlers = 16;  // API + 정적 파일
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return ret;
    }

    // URI 등록
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_css);
    httpd_register_uri_handler(s_server, &uri_js);
    httpd_register_uri_handler(s_server, &uri_api_status);
    httpd_register_uri_handler(s_server, &uri_api_reboot);
    httpd_register_uri_handler(s_server, &uri_api_config_network_ap);
    httpd_register_uri_handler(s_server, &uri_api_config_network_wifi);
    httpd_register_uri_handler(s_server, &uri_api_config_network_ethernet);

    // 이벤트 구독
    event_bus_subscribe(EVT_INFO_UPDATED, onSystemInfoEvent);
    event_bus_subscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_subscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);

    // 설정 데이터 요청 (초기 캐시 populate)
    event_bus_publish(EVT_CONFIG_DATA_REQUEST, nullptr, 0);

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == nullptr) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping web server");

    // 이벤트 구독 해제
    event_bus_unsubscribe(EVT_INFO_UPDATED, onSystemInfoEvent);
    event_bus_unsubscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);

    // 캐시 무효화
    s_cache.system_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.config_valid = false;

    esp_err_t ret = httpd_stop(s_server);
    s_server = nullptr;
    return ret;
}

bool web_server_is_running(void)
{
    return s_server != nullptr;
}

} // extern "C"
