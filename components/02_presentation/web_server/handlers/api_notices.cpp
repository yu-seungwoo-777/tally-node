/**
 * @file api_notices.cpp
 * @brief API Notices 핸들러 구현
 */

#include "api_notices.h"
#include "web_server_helpers.h"
#include "t_log.h"
#include "esp_http_client.h"
#include <cstring>
#include <malloc.h>

static const char* TAG = "02_WS_Notices";

extern "C" {

esp_err_t http_notices_event_handler(esp_http_client_event_t *evt)
{
    http_response_context_t* ctx = (http_response_context_t*)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx->bytes_written < ctx->buffer_size) {
                size_t copy_len = evt->data_len;
                if (copy_len > ctx->buffer_size - ctx->bytes_written) {
                    copy_len = ctx->buffer_size - ctx->bytes_written;
                }
                memcpy(ctx->buffer + ctx->bytes_written, evt->data, copy_len);
                ctx->bytes_written += copy_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t api_notices_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // HTTP 응답 버퍼 (스택 오버플로우 방지: 힙 할당)
    char* response_buffer = (char*)malloc(2048);
    if (!response_buffer) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"notices\":[]}");
        return ESP_OK;
    }
    memset(response_buffer, 0, 2048);

    // 이벤트 핸들러용 컨텍스트
    http_response_context_t context = { response_buffer, 2047, 0 };

    // esp_http_client로 외부 API 호출
    esp_http_client_config_t config = {};
    config.url = "http://tally-node.duckdns.org/api/notices";
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 5000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.user_agent = "ESP32-Tally-Node";
    config.keep_alive_enable = true;
    config.event_handler = http_notices_event_handler;
    config.user_data = &context;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(response_buffer);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"notices\":[]}");
        return ESP_OK;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK && context.bytes_written > 0) {
        response_buffer[context.bytes_written] = '\0';
        T_LOGI(TAG, "Notices fetched successfully: %d bytes", context.bytes_written);
    } else {
        if (err != ESP_OK) {
            T_LOGW(TAG, "Notices fetch failed: %s", esp_err_to_name(err));
        } else {
            T_LOGW(TAG, "No response data");
        }
        snprintf(response_buffer, 2048, "{\"success\":false,\"notices\":[]}");
    }

    esp_http_client_cleanup(client);

    // 클라이언트에게 전달
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response_buffer);

    free(response_buffer);
    return ESP_OK;
}

} // extern "C"
