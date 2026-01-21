/**
 * @file api_status.cpp
 * @brief API Status 핸들러 구현
 */

#include "api_status.h"
#include "web_server_json.h"
#include "web_server_helpers.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "esp_system.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "02_WS_Status";

extern "C" {

// ============================================================================
// API 핸들러 구현
// ============================================================================

esp_err_t api_status_handler(httpd_req_t* req)
{
    T_LOGD(TAG, "GET /api/status");
    web_server_set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return web_server_send_json_error(req, "Memory allocation failed");
    }

    // Network
    cJSON* network = cJSON_CreateObject();
    if (network) {
        cJSON* ap = web_server_json_create_network_ap();
        if (ap) {
            cJSON_AddItemToObject(network, "ap", ap);
        }

        cJSON* wifi = web_server_json_create_network_wifi();
        if (wifi) {
            cJSON_AddItemToObject(network, "wifi", wifi);
        }

        cJSON* ethernet = web_server_json_create_network_ethernet();
        if (ethernet) {
            cJSON_AddItemToObject(network, "ethernet", ethernet);
        }

        cJSON_AddItemToObject(root, "network", network);
    }

    // Switcher
    cJSON* switcher = web_server_json_create_switcher();
    if (switcher) {
        cJSON_AddItemToObject(root, "switcher", switcher);
    }

    // System
    cJSON* system = web_server_json_create_system();
    if (system) {
        cJSON_AddItemToObject(root, "system", system);
    }

    // Broadcast
    cJSON* broadcast = web_server_json_create_broadcast();
    if (broadcast) {
        cJSON_AddItemToObject(root, "broadcast", broadcast);
    }

    // License
    cJSON* license = web_server_json_create_license();
    if (license) {
        cJSON_AddItemToObject(root, "license", license);
    }

    // LED Colors
    cJSON* led = web_server_json_create_led_colors();
    if (led) {
        cJSON_AddItemToObject(root, "led", led);
    }

    return web_server_send_json_response(req, root);
}

esp_err_t api_factory_reset_handler(httpd_req_t* req)
{
    T_LOGI(TAG, "POST /api/factory-reset");
    web_server_set_cors_headers(req);

    // 이벤트 버스로 factory reset 요청 발행
    esp_err_t ret = event_bus_publish(EVT_FACTORY_RESET_REQUEST, NULL, 0);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Factory reset event publish failed: %s", esp_err_to_name(ret));
        return web_server_send_json_internal_error(req, "Failed to publish factory reset event");
    }

    T_LOGI(TAG, "Factory reset event published");

    // 성공 응답 전송
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "status", "ok");
        cJSON_AddStringToObject(json, "message", "Factory reset in progress...");
    }
    web_server_send_json_response(req, json);

    // ConfigService가 이벤트를 처리하고 재부팅함
    return ESP_OK;
}

} // extern "C"
