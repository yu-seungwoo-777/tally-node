/**
 * @file api_status.cpp
 * @brief API Status 핸들러 구현
 */

#include "api_status.h"
#include "web_server_json.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "esp_system.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "02_WebSvr_Status";

// ============================================================================
// CORS 헬퍼 함수
// ============================================================================

/**
 * @brief CORS 헤더 설정
 * @param req HTTP 요청 핸들러
 * @details Cross-Origin Resource Sharing 헤더를 설정하여 웹 브라우저에서의 API 접근을 허용합니다
 */
static void set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

extern "C" {

// ============================================================================
// API 핸들러 구현
// ============================================================================

esp_err_t api_status_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
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

    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_reboot_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

esp_err_t api_reboot_broadcast_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    // 브로드캐스트 ID (0xFF 0xFF)
    uint8_t broadcast_id[LORA_DEVICE_ID_LEN] = {0xFF, 0xFF};

    // 이벤트 발행 (3회 송신)
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++) {
        ret = event_bus_publish(EVT_DEVICE_REBOOT_REQUEST, broadcast_id, LORA_DEVICE_ID_LEN);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "Broadcast reboot failed (attempt %d): %d", i + 1, ret);
            httpd_resp_set_status(req, HTTPD_500);
            httpd_resp_sendstr(req, "{\"error\":\"Failed to send broadcast reboot\"}");
            return ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        T_LOGI(TAG, "Broadcast reboot command sent 3 times, TX rebooting in 500ms");
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Broadcast reboot sent (3x), TX rebooting...\"}");

        // 3회 송신 후 500ms 대기 후 TX 재부팅
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    return ESP_OK;
}

} // extern "C"
