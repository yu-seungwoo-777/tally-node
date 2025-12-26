/**
 * @file web_server.cpp
 * @brief Web Server Implementation for Tally Node
 *
 * Alpine.js + DaisyUI 기반 웹 인터페이스를 위한
 * HTTP/WebSocket 서버 구현
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "web_server";

// 웹 서버 상태
static httpd_handle_t g_server = NULL;
static bool g_running = false;

// 정적 파일 (임베디드) - build 시 생성됨
#ifdef INDEX_HTML_H
#include "index_html.h"
#endif

extern "C" {

/**
 * @brief index.html 핸들러
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving index.html");

#ifdef INDEX_HTML_H
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_data, index_html_len);
#else
    const char* msg = "Static files not embedded. Run 'npm run deploy' in web/ folder.";
    httpd_resp_send(req, msg, strlen(msg));
    ESP_LOGW(TAG, "%s", msg);
#endif

    return ESP_OK;
}

/**
 * @brief API 상태 핸들러
 */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    // TODO: 실제 상태 조회 - event_bus 또는 서비스에서 가져오기
    const char* json = "{"
        "\"channels\": [\"off\",\"off\",\"off\",\"off\",\"off\",\"off\",\"off\",\"off\","
        "\"off\",\"off\",\"off\",\"off\",\"off\",\"off\",\"off\",\"off\"],"
        "\"lora\": {\"rssi\": -45, \"snr\": 12, \"tx\": 123, \"rx\": 456},"
        "\"network\": {\"ip\": \"192.168.1.100\", \"mode\": \"TX\", \"wifiConnected\": true},"
        "\"switcher\": {\"primary\": \"ATEM\", \"primaryConnected\": true, "
        "\"secondary\": \"vMix\", \"secondaryConnected\": false},"
        "\"system\": {\"uptime\": 3600, \"freeHeap\": 250000, \"wifiMode\": \"AP+STA\", \"version\": \"0.1.0\"}"
    "}";

    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

/**
 * @brief URI 등록
 */
static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
};

static const httpd_uri_t api_status_uri = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = api_status_handler,
    .user_ctx = NULL
};

} // extern "C"

esp_err_t web_server_init(void)
{
    if (g_server != NULL) {
        ESP_LOGW(TAG, "Web server already initialized");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = tskIDLE_PRIORITY + 3;
    config.stack_size = 8192;
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);

    esp_err_t ret = httpd_start(&g_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return ret;
    }

    // URI 등록
    httpd_register_uri_handler(g_server, &index_uri);
    httpd_register_uri_handler(g_server, &api_status_uri);

    ESP_LOGI(TAG, "Web server started successfully");
    g_running = true;

    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    return web_server_init();
}

esp_err_t web_server_stop(void)
{
    if (g_server == NULL) {
        ESP_LOGW(TAG, "Web server not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping web server");
    esp_err_t ret = httpd_stop(g_server);
    g_server = NULL;
    g_running = false;

    return ret;
}

bool web_server_is_running(void)
{
    return g_running;
}

void web_server_broadcast_tally(const uint8_t *channels, size_t count)
{
    // TODO: WebSocket 구현 후 브로드캐스트
    ESP_LOGD(TAG, "Broadcast tally state (not implemented yet)");
}

void web_server_broadcast_lora(int16_t rssi, int8_t snr, uint32_t tx_packets, uint32_t rx_packets)
{
    // TODO: WebSocket 구현 후 브로드캐스트
    ESP_LOGD(TAG, "Broadcast LoRa status (not implemented yet)");
}
