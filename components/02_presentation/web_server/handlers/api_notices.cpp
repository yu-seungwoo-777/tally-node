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

// ISRG Root X1 인증서 (Let's Encrypt Root CA, 2035년까지 유효)
static const char* ISRG_ROOT_X1_CERT =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

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

    T_LOGD(TAG, "notices:start");

    // esp_http_client로 외부 API 호출
    esp_http_client_config_t config = {};
    config.url = "https://tally-node.com/api/notices";
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 15000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.user_agent = "ESP32-Tally-Node/1.0";
    config.keep_alive_enable = true;
    config.event_handler = http_notices_event_handler;
    config.user_data = &context;
    // HTTPS/TLS 설정 - ISRG Root X1 인증서 사용
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = ISRG_ROOT_X1_CERT;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;

    T_LOGD(TAG, "notices:init_client");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        T_LOGE(TAG, "notices:fail:init");
        free(response_buffer);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"notices\":[]}");
        return ESP_OK;
    }

    T_LOGD(TAG, "notices:performing...");
    esp_err_t err = esp_http_client_perform(client);
    T_LOGD(TAG, "notices:done:0x%x", err);

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
