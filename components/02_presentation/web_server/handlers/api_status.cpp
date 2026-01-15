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

esp_err_t api_reboot_handler(httpd_req_t* req)
{
    T_LOGI(TAG, "POST /api/reboot");
    web_server_set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

esp_err_t api_reboot_broadcast_handler(httpd_req_t* req)
{
    T_LOGI(TAG, "POST /api/reboot/broadcast");
    web_server_set_cors_headers(req);

    // 브로드캐스트 ID (0xFF 0xFF)
    uint8_t broadcast_id[LORA_DEVICE_ID_LEN] = {0xFF, 0xFF};

    // 이벤트 발행 (3회 송신)
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++) {
        ret = event_bus_publish(EVT_DEVICE_REBOOT_REQUEST, broadcast_id, LORA_DEVICE_ID_LEN);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "Broadcast reboot failed (attempt %d): %d", i + 1, ret);
            return web_server_send_json_internal_error(req, "Failed to send broadcast reboot");
        }
    }

    T_LOGI(TAG, "Broadcast reboot command sent 3 times, TX rebooting in 500ms");

    // 성공 응답 전송
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "status", "ok");
        cJSON_AddStringToObject(json, "message", "Broadcast reboot sent (3x), TX rebooting...");
    }
    web_server_send_json_response(req, json);

    // 3회 송신 후 500ms 대기 후 TX 재부팅
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK;
}

} // extern "C"
