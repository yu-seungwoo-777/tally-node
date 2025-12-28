/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API (Event-based)
 */

#include "web_server.h"
#include "config_service.h"
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

    lora_rssi_event_t lora_rf;        // EVT_LORA_RSSI_CHANGED
    bool lora_rf_valid;

    switcher_status_event_t switcher; // EVT_SWITCHER_STATUS_CHANGED
    bool switcher_valid;

    network_status_event_t network;   // EVT_NETWORK_STATUS_CHANGED
    bool network_valid;

    tally_event_data_t tally;         // EVT_TALLY_STATE_CHANGED
    bool tally_valid;
} web_server_data_t;

static web_server_data_t s_cache;

// 초기화를 위한 정적 함수
static void init_cache(void)
{
    memset(&s_cache, 0, sizeof(s_cache));
    s_cache.system_valid = false;
    s_cache.lora_rf_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.tally_valid = false;
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
 * @brief LoRa RSSI 이벤트 핸들러 (EVT_LORA_RSSI_CHANGED)
 */
static esp_err_t onLoraRssiEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_rssi_event_t* rf = (const lora_rssi_event_t*)event->data;
    memcpy(&s_cache.lora_rf, rf, sizeof(lora_rssi_event_t));
    s_cache.lora_rf_valid = true;

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
 * @brief Tally 상태 이벤트 핸들러 (EVT_TALLY_STATE_CHANGED)
 */
static esp_err_t onTallyStateEvent(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const tally_event_data_t* tally = (const tally_event_data_t*)event->data;
    memcpy(&s_cache.tally, tally, sizeof(tally_event_data_t));
    s_cache.tally_valid = true;

    return ESP_OK;
}

// ============================================================================
// Tally 상태 헬퍼 함수
// ============================================================================

/**
 * @brief packed 데이터에서 채널 상태 가져오기
 * @param tally tally 데이터
 * @param channel 채널 번호 (1-20)
 * @return 0=off, 1=live, 2=preview, 3=live+preview
 */
static uint8_t get_tally_state(const tally_event_data_t* tally, uint8_t channel)
{
    if (!tally || channel < 1 || channel > tally->channel_count) {
        return 0;
    }

    // packed 데이터: 2비트 per channel
    // 00=off, 01=live, 10=preview, 11=both
    uint8_t byte_idx = (channel - 1) / 4;
    uint8_t bit_idx = ((channel - 1) % 4) * 2;

    if (byte_idx >= 8) {
        return 0;
    }

    return (tally->tally_data[byte_idx] >> bit_idx) & 0x03;
}

/**
 * @brief 채널 상태 문자열 반환
 */
static const char* tally_state_to_string(uint8_t state)
{
    switch (state) {
        case 1:
        case 3:
            return "live";
        case 2:
            return "preview";
        default:
            return "off";
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

    // LoRa (EVT_LORA_RSSI_CHANGED)
    cJSON* lora = cJSON_CreateObject();
    if (s_cache.lora_rf_valid) {
        cJSON_AddNumberToObject(lora, "rssi", s_cache.lora_rf.rssi);
        cJSON_AddNumberToObject(lora, "snr", s_cache.lora_rf.snr);
        cJSON_AddBoolToObject(lora, "running", s_cache.lora_rf.is_running);
        cJSON_AddNumberToObject(lora, "chipType", s_cache.lora_rf.chip_type);
        cJSON_AddNumberToObject(lora, "frequency", s_cache.lora_rf.frequency);
    } else {
        cJSON_AddNumberToObject(lora, "rssi", -120);
        cJSON_AddNumberToObject(lora, "snr", 0);
        cJSON_AddBoolToObject(lora, "running", false);
        cJSON_AddNumberToObject(lora, "chipType", 0);
        cJSON_AddNumberToObject(lora, "frequency", 0);
    }
    // tx/rx는 이벤트에 없으므로 0으로 표시
    cJSON_AddNumberToObject(lora, "tx", 0);
    cJSON_AddNumberToObject(lora, "rx", 0);
    cJSON_AddItemToObject(root, "lora", lora);

    // Network (EVT_NETWORK_STATUS_CHANGED)
    cJSON* network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "mode", "TX");

    cJSON* wifi = cJSON_CreateObject();
    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(wifi, "connected", s_cache.network.sta_connected);
        cJSON_AddStringToObject(wifi, "ssid", s_cache.network.sta_ssid);
        cJSON_AddStringToObject(wifi, "ip", s_cache.network.sta_connected ? s_cache.network.sta_ip : "--");
    } else {
        cJSON_AddBoolToObject(wifi, "connected", false);
        cJSON_AddStringToObject(wifi, "ssid", "--");
        cJSON_AddStringToObject(wifi, "ip", "--");
    }
    cJSON_AddItemToObject(network, "wifi", wifi);

    cJSON* ethernet = cJSON_CreateObject();
    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(ethernet, "connected", s_cache.network.eth_connected);
        cJSON_AddStringToObject(ethernet, "ip", s_cache.network.eth_connected ? s_cache.network.eth_ip : "--");
    } else {
        cJSON_AddBoolToObject(ethernet, "connected", false);
        cJSON_AddStringToObject(ethernet, "ip", "--");
    }
    cJSON_AddItemToObject(network, "ethernet", ethernet);

    cJSON* wifi_ap = cJSON_CreateObject();
    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(wifi_ap, "enabled", s_cache.network.ap_enabled);
        cJSON_AddStringToObject(wifi_ap, "ssid", s_cache.network.ap_ssid);
        cJSON_AddStringToObject(wifi_ap, "ip", s_cache.network.ap_enabled ? s_cache.network.ap_ip : "--");
    } else {
        cJSON_AddBoolToObject(wifi_ap, "enabled", false);
        cJSON_AddStringToObject(wifi_ap, "ssid", "--");
        cJSON_AddStringToObject(wifi_ap, "ip", "--");
    }
    cJSON_AddItemToObject(network, "wifiAp", wifi_ap);

    cJSON_AddItemToObject(root, "network", network);

    // Switcher (EVT_SWITCHER_STATUS_CHANGED)
    cJSON* switcher = cJSON_CreateObject();
    if (s_cache.switcher_valid) {
        cJSON_AddBoolToObject(switcher, "dualEnabled", s_cache.switcher.dual_mode);
        cJSON_AddStringToObject(switcher, "s1Type", s_cache.switcher.s1_type);
        cJSON_AddStringToObject(switcher, "s1Ip", s_cache.switcher.s1_ip);
        cJSON_AddNumberToObject(switcher, "s1Port", s_cache.switcher.s1_port);
        cJSON_AddBoolToObject(switcher, "s1Connected", s_cache.switcher.s1_connected);
        cJSON_AddStringToObject(switcher, "s2Type", s_cache.switcher.s2_type);
        cJSON_AddStringToObject(switcher, "s2Ip", s_cache.switcher.s2_ip);
        cJSON_AddNumberToObject(switcher, "s2Port", s_cache.switcher.s2_port);
        cJSON_AddBoolToObject(switcher, "s2Connected", s_cache.switcher.s2_connected);
    } else {
        cJSON_AddBoolToObject(switcher, "dualEnabled", false);
        cJSON_AddStringToObject(switcher, "s1Type", "--");
        cJSON_AddStringToObject(switcher, "s1Ip", "--");
        cJSON_AddNumberToObject(switcher, "s1Port", 0);
        cJSON_AddBoolToObject(switcher, "s1Connected", false);
        cJSON_AddStringToObject(switcher, "s2Type", "--");
        cJSON_AddStringToObject(switcher, "s2Ip", "--");
        cJSON_AddNumberToObject(switcher, "s2Port", 0);
        cJSON_AddBoolToObject(switcher, "s2Connected", false);
    }
    cJSON_AddItemToObject(root, "switcher", switcher);

    // Tally (EVT_TALLY_STATE_CHANGED)
    cJSON* tally_json = cJSON_CreateObject();
    cJSON* channels = cJSON_CreateArray();

    uint8_t channel_count = s_cache.tally_valid ? s_cache.tally.channel_count : 0;
    for (int i = 0; i < 20; i++) {
        uint8_t state = 0;
        if (s_cache.tally_valid && i < channel_count) {
            state = get_tally_state(&s_cache.tally, i + 1);
        }
        cJSON_AddItemToArray(channels, cJSON_CreateString(tally_state_to_string(state)));
    }

    cJSON_AddItemToObject(tally_json, "channels", channels);
    cJSON_AddNumberToObject(tally_json, "source", s_cache.tally_valid ? s_cache.tally.source : 0);
    cJSON_AddItemToObject(root, "tally", tally_json);

    // System (EVT_INFO_UPDATED)
    cJSON* system = cJSON_CreateObject();
    if (s_cache.system_valid) {
        cJSON_AddStringToObject(system, "deviceId", s_cache.system.device_id);
        cJSON_AddNumberToObject(system, "battery", s_cache.system.battery);
        // 소수점 한 자리로 제한 (4.2, 52.8)
        cJSON_AddNumberToObject(system, "voltage", (round(s_cache.system.voltage * 10) / 10));
        cJSON_AddNumberToObject(system, "temperature", (round(s_cache.system.temperature * 10) / 10));
        cJSON_AddNumberToObject(system, "uptime", s_cache.system.uptime);
        cJSON_AddBoolToObject(system, "stopped", s_cache.system.stopped);
    } else {
        cJSON_AddStringToObject(system, "deviceId", "0000");
        cJSON_AddNumberToObject(system, "battery", 0);
        cJSON_AddNumberToObject(system, "voltage", 0);
        cJSON_AddNumberToObject(system, "temperature", 0);
        cJSON_AddNumberToObject(system, "uptime", 0);
        cJSON_AddBoolToObject(system, "stopped", false);
    }
    cJSON_AddNumberToObject(system, "freeHeap", esp_get_free_heap_size());
    cJSON_AddStringToObject(system, "version", "0.1.0");
    cJSON_AddItemToObject(root, "system", system);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief GET /api/config - 설정 조회
 */
static esp_err_t api_config_get_handler(httpd_req_t* req)
{
    config_all_t config;
    esp_err_t ret = config_service_load_all(&config);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load config");
        return ESP_FAIL;
    }

    cJSON* root = cJSON_CreateObject();

    // Network
    cJSON* network = cJSON_CreateObject();

    cJSON* wifi_ap = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_ap, "ssid", config.wifi_ap.ssid);
    cJSON_AddNumberToObject(wifi_ap, "channel", config.wifi_ap.channel);
    cJSON_AddBoolToObject(wifi_ap, "enabled", config.wifi_ap.enabled);
    cJSON_AddItemToObject(network, "wifiAp", wifi_ap);

    cJSON* wifi_sta = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_sta, "ssid", config.wifi_sta.ssid);
    cJSON_AddBoolToObject(wifi_sta, "enabled", config.wifi_sta.enabled);
    cJSON_AddItemToObject(network, "wifiSta", wifi_sta);

    cJSON* ethernet = cJSON_CreateObject();
    cJSON_AddBoolToObject(ethernet, "dhcp", config.ethernet.dhcp_enabled);
    cJSON_AddStringToObject(ethernet, "staticIp", config.ethernet.static_ip);
    cJSON_AddStringToObject(ethernet, "netmask", config.ethernet.static_netmask);
    cJSON_AddStringToObject(ethernet, "gateway", config.ethernet.static_gateway);
    cJSON_AddBoolToObject(ethernet, "enabled", config.ethernet.enabled);
    cJSON_AddItemToObject(network, "ethernet", ethernet);

    cJSON_AddItemToObject(root, "network", network);

    // Switcher
    cJSON* switcher = cJSON_CreateObject();

    cJSON* primary = cJSON_CreateObject();
    const char* type_str = (config.primary.type == 0) ? "ATEM" :
                          (config.primary.type == 1) ? "OBS" : "vMix";
    cJSON_AddStringToObject(primary, "type", type_str);
    cJSON_AddStringToObject(primary, "ip", config.primary.ip);
    cJSON_AddNumberToObject(primary, "port", config.primary.port);
    cJSON_AddNumberToObject(primary, "interface", config.primary.interface);
    cJSON_AddNumberToObject(primary, "cameraLimit", config.primary.camera_limit);
    cJSON_AddItemToObject(switcher, "primary", primary);

    cJSON* secondary = cJSON_CreateObject();
    type_str = (config.secondary.type == 0) ? "ATEM" :
               (config.secondary.type == 1) ? "OBS" : "vMix";
    cJSON_AddStringToObject(secondary, "type", type_str);
    cJSON_AddStringToObject(secondary, "ip", config.secondary.ip);
    cJSON_AddNumberToObject(secondary, "port", config.secondary.port);
    cJSON_AddNumberToObject(secondary, "interface", config.secondary.interface);
    cJSON_AddNumberToObject(secondary, "cameraLimit", config.secondary.camera_limit);
    cJSON_AddItemToObject(switcher, "secondary", secondary);

    cJSON_AddBoolToObject(switcher, "dualEnabled", config.dual_enabled);
    cJSON_AddNumberToObject(switcher, "secondaryOffset", config.secondary_offset);
    cJSON_AddItemToObject(root, "switcher", switcher);

    // Device (LoRa RF)
    cJSON* device = cJSON_CreateObject();
    cJSON_AddNumberToObject(device, "brightness", config.device.brightness);
    cJSON_AddNumberToObject(device, "cameraId", config.device.camera_id);

    cJSON* rf = cJSON_CreateObject();
    cJSON_AddNumberToObject(rf, "frequency", config.device.rf.frequency);
    cJSON_AddNumberToObject(rf, "syncWord", config.device.rf.sync_word);
    cJSON_AddNumberToObject(rf, "spreadingFactor", config.device.rf.sf);
    cJSON_AddNumberToObject(rf, "codingRate", config.device.rf.cr);
    cJSON_AddNumberToObject(rf, "bandwidth", config.device.rf.bw);
    cJSON_AddNumberToObject(rf, "txPower", config.device.rf.tx_power);
    cJSON_AddItemToObject(device, "rf", rf);
    cJSON_AddItemToObject(root, "device", device);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief POST /api/config/path - 설정 저장 (와일드카드 매칭)
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

    esp_err_t result = ESP_OK;
    const char* response_msg = "OK";

    // 경로별 처리
    if (strncmp(path, "device/brightness", 16) == 0) {
        cJSON* item = cJSON_GetObjectItem(root, "value");
        if (item && cJSON_IsNumber(item)) {
            uint8_t brightness = (uint8_t)item->valueint;
            result = config_service_set_brightness(brightness);
        } else {
            result = ESP_FAIL;
            response_msg = "Missing or invalid 'value'";
        }
    }
    else if (strncmp(path, "device/camera_id", 15) == 0) {
        cJSON* item = cJSON_GetObjectItem(root, "value");
        if (item && cJSON_IsNumber(item)) {
            uint8_t camera_id = (uint8_t)item->valueint;
            result = config_service_set_camera_id(camera_id);
        } else {
            result = ESP_FAIL;
            response_msg = "Missing or invalid 'value'";
        }
    }
    else if (strncmp(path, "device/rf", 9) == 0) {
        cJSON* freq = cJSON_GetObjectItem(root, "frequency");
        cJSON* sync = cJSON_GetObjectItem(root, "syncWord");
        if (freq && sync && cJSON_IsNumber(freq) && cJSON_IsNumber(sync)) {
            result = config_service_set_rf((float)freq->valuedouble,
                                          (uint8_t)sync->valueint);
        } else {
            result = ESP_FAIL;
            response_msg = "Missing 'frequency' or 'syncWord'";
        }
    }
    else if (strncmp(path, "switcher/primary", 16) == 0) {
        config_switcher_t sw;
        config_service_get_primary(&sw);

        cJSON* type = cJSON_GetObjectItem(root, "type");
        cJSON* ip = cJSON_GetObjectItem(root, "ip");
        cJSON* port = cJSON_GetObjectItem(root, "port");

        if (type) {
            const char* type_str = type->valuestring;
            sw.type = (strcmp(type_str, "ATEM") == 0) ? 0 :
                      (strcmp(type_str, "OBS") == 0) ? 1 : 2;
        }
        if (ip && cJSON_IsString(ip)) {
            strncpy(sw.ip, ip->valuestring, sizeof(sw.ip) - 1);
        }
        if (port && cJSON_IsNumber(port)) {
            sw.port = (uint16_t)port->valueint;
        }
        result = config_service_set_primary(&sw);
    }
    else if (strncmp(path, "switcher/secondary", 18) == 0) {
        config_switcher_t sw;
        config_service_get_secondary(&sw);

        cJSON* type = cJSON_GetObjectItem(root, "type");
        cJSON* ip = cJSON_GetObjectItem(root, "ip");
        cJSON* port = cJSON_GetObjectItem(root, "port");

        if (type) {
            const char* type_str = type->valuestring;
            sw.type = (strcmp(type_str, "ATEM") == 0) ? 0 :
                      (strcmp(type_str, "OBS") == 0) ? 1 : 2;
        }
        if (ip && cJSON_IsString(ip)) {
            strncpy(sw.ip, ip->valuestring, sizeof(sw.ip) - 1);
        }
        if (port && cJSON_IsNumber(port)) {
            sw.port = (uint16_t)port->valueint;
        }
        result = config_service_set_secondary(&sw);
    }
    else if (strncmp(path, "switcher/dual", 13) == 0) {
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
        cJSON* offset = cJSON_GetObjectItem(root, "offset");

        if (enabled && cJSON_IsBool(enabled)) {
            bool dual_enabled = cJSON_IsTrue(enabled);
            result = config_service_set_dual_enabled(dual_enabled);
        }
        if (offset && cJSON_IsNumber(offset)) {
            config_service_set_secondary_offset((uint8_t)offset->valueint);
        }
    }
    else if (strncmp(path, "network/wifi", 13) == 0) {
        config_wifi_sta_t wifi;
        config_service_get_wifi_sta(&wifi);

        cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
        cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

        if (ssid && cJSON_IsString(ssid)) {
            strncpy(wifi.ssid, ssid->valuestring, sizeof(wifi.ssid) - 1);
        }
        if (enabled) {
            wifi.enabled = cJSON_IsTrue(enabled);
        } else {
            wifi.enabled = true;
        }

        result = config_service_set_wifi_sta(&wifi);
    }
    else if (strncmp(path, "network/ethernet", 16) == 0) {
        config_ethernet_t eth;
        config_service_get_ethernet(&eth);

        cJSON* ip = cJSON_GetObjectItem(root, "staticIp");
        cJSON* gateway = cJSON_GetObjectItem(root, "gateway");
        cJSON* netmask = cJSON_GetObjectItem(root, "netmask");
        cJSON* dhcp = cJSON_GetObjectItem(root, "dhcp");

        if (ip && cJSON_IsString(ip)) {
            strncpy(eth.static_ip, ip->valuestring, sizeof(eth.static_ip) - 1);
        }
        if (gateway && cJSON_IsString(gateway)) {
            strncpy(eth.static_gateway, gateway->valuestring, sizeof(eth.static_gateway) - 1);
        }
        if (netmask && cJSON_IsString(netmask)) {
            strncpy(eth.static_netmask, netmask->valuestring, sizeof(eth.static_netmask) - 1);
        }
        if (dhcp) {
            eth.dhcp_enabled = cJSON_IsTrue(dhcp);
        }

        result = config_service_set_ethernet(&eth);
    }
    else {
        result = ESP_FAIL;
        response_msg = "Unknown config path";
    }

    cJSON_Delete(root);

    // 응답
    if (result == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_set_type(req, "application/json");
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"%s\"}", response_msg);
        httpd_resp_send(req, resp, strlen(resp));
    }

    return result;
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

static const httpd_uri_t uri_api_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = api_config_get_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_POST,
    .handler = api_reboot_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_post = {
    .uri = "/api/config/*",
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
    config.max_uri_handlers = 12;  // API + 정적 파일
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
    httpd_register_uri_handler(s_server, &uri_api_config);
    httpd_register_uri_handler(s_server, &uri_api_reboot);
    httpd_register_uri_handler(s_server, &uri_api_config_post);

    // 이벤트 구독
    event_bus_subscribe(EVT_INFO_UPDATED, onSystemInfoEvent);
    event_bus_subscribe(EVT_LORA_RSSI_CHANGED, onLoraRssiEvent);
    event_bus_subscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_subscribe(EVT_TALLY_STATE_CHANGED, onTallyStateEvent);

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
    event_bus_unsubscribe(EVT_LORA_RSSI_CHANGED, onLoraRssiEvent);
    event_bus_unsubscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_unsubscribe(EVT_TALLY_STATE_CHANGED, onTallyStateEvent);

    // 캐시 무효화
    s_cache.system_valid = false;
    s_cache.lora_rf_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.tally_valid = false;

    esp_err_t ret = httpd_stop(s_server);
    s_server = nullptr;
    return ret;
}

bool web_server_is_running(void)
{
    return s_server != nullptr;
}

} // extern "C"
