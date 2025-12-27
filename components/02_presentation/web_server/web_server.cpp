/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API
 */

#include "web_server.h"
#include "config_service.h"
#include "lora_service.h"
#include "network_service.h"
#include "switcher_service.h"
#include "TallyTypes.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "WebServer";
static httpd_handle_t s_server = nullptr;

// 외부에서 제공받을 switcher_service 핸들
// main.cpp에서 web_server_set_switcher_handle()로 설정
static switcher_service_handle_t s_switcher_handle = nullptr;

// ============================================================================
// 정적 파일 (임베디드) - web/ 폴더에서 build 시 생성됨
// ============================================================================
#include "static_files.h"

// ============================================================================
// API 핸들러
// ============================================================================

/**
 * @brief GET /api/status - 전체 상태 반환
 */
static esp_err_t api_status_handler(httpd_req_t* req)
{
    cJSON* root = cJSON_CreateObject();

    // LoRa
    lora_service_status_t lora_status = lora_service_get_status();
    cJSON* lora = cJSON_CreateObject();
    cJSON_AddNumberToObject(lora, "rssi", lora_status.rssi);
    cJSON_AddNumberToObject(lora, "snr", lora_status.snr);
    cJSON_AddNumberToObject(lora, "tx", lora_status.packets_sent);
    cJSON_AddNumberToObject(lora, "rx", lora_status.packets_received);
    cJSON_AddBoolToObject(lora, "running", lora_status.is_running);
    cJSON_AddItemToObject(root, "lora", lora);

    // Network
    network_status_t net_status = network_service_get_status();
    cJSON* network = cJSON_CreateObject();
    cJSON_AddStringToObject(network, "mode", "TX");

    cJSON* wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "connected", net_status.wifi_sta.connected);
    cJSON_AddStringToObject(wifi, "ip", net_status.wifi_sta.connected ? net_status.wifi_sta.ip : "--");
    cJSON_AddItemToObject(network, "wifi", wifi);

    cJSON* ethernet = cJSON_CreateObject();
    cJSON_AddBoolToObject(ethernet, "connected", net_status.ethernet.connected);
    cJSON_AddStringToObject(ethernet, "ip", net_status.ethernet.connected ? net_status.ethernet.ip : "--");
    cJSON_AddItemToObject(network, "ethernet", ethernet);
    cJSON_AddItemToObject(root, "network", network);

    // Switcher & Tally
    cJSON* switcher = cJSON_CreateObject();
    bool switcher_connected = false;
    const char* switcher_type = "--";
    uint8_t program = 0;
    uint8_t preview = 0;

    // Tally JSON (먼저 생성)
    cJSON* tally_json = cJSON_CreateObject();
    cJSON* channels = cJSON_CreateArray();

    if (s_switcher_handle != nullptr) {
        switcher_status_t primary_status = switcher_service_get_switcher_status(
            s_switcher_handle, SWITCHER_ROLE_PRIMARY);
        switcher_connected = (primary_status.state == CONNECTION_STATE_CONNECTED);
        // TODO: switcher_type는 config_service에서 가져와야 함

        packed_data_t packed_tally = switcher_service_get_combined_tally(s_switcher_handle);
        // Packed 데이터에서 program/preview 추출 (채널 1만 예시)
        if (packed_tally.channel_count > 0) {
            uint8_t state1 = switcher_service_get_tally_state(&packed_tally, 1);
            program = (state1 == 1 || state1 == 3) ? 1 : 0;
            preview = (state1 == 2 || state1 == 3) ? 1 : 0;
        }

        // Tally 채널 배열
        for (int i = 1; i <= 16; i++) {
            uint8_t state = switcher_service_get_tally_state(&packed_tally, i);
            const char* state_str = (state == 1 || state == 3) ? "live" :
                                   (state == 2 || state == 3) ? "preview" : "off";
            cJSON_AddItemToArray(channels, cJSON_CreateString(state_str));
        }
    } else {
        // 핸들이 없으면 모두 off
        for (int i = 0; i < 16; i++) {
            cJSON_AddItemToArray(channels, cJSON_CreateString("off"));
        }
    }

    cJSON_AddItemToObject(tally_json, "channels", channels);
    cJSON_AddNumberToObject(tally_json, "source", 0);  // TODO: 듀얼모드 시 소스 표시
    cJSON_AddItemToObject(root, "tally", tally_json);

    cJSON_AddBoolToObject(switcher, "connected", switcher_connected);
    cJSON_AddStringToObject(switcher, "type", switcher_type);
    cJSON_AddNumberToObject(switcher, "program", program);
    cJSON_AddNumberToObject(switcher, "preview", preview);
    cJSON_AddItemToObject(root, "switcher", switcher);

    // System
    cJSON* system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "uptime", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
    cJSON_AddNumberToObject(system, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(system, "battery", 0);  // TODO: battery service
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

/**
 * @brief Switcher Service 핸들 설정
 * @param handle switcher_service 핸들
 */
void web_server_set_switcher_handle(switcher_service_handle_t handle)
{
    s_switcher_handle = handle;
}

esp_err_t web_server_init(void)
{
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

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == nullptr) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping web server");

    esp_err_t ret = httpd_stop(s_server);
    s_server = nullptr;
    return ret;
}

bool web_server_is_running(void)
{
    return s_server != nullptr;
}

} // extern "C"
