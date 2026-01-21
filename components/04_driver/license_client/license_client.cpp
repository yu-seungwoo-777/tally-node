/**
 * @file license_client.cpp
 * @brief 라이센스 서버 HTTP 클라이언트 구현
 */

#include "license_client.h"
#include "t_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "04_LicenseCli";

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

// ============================================================================
// HTTP 응답 데이터 컨텍스트
// ============================================================================

typedef struct {
    char* buffer;
    size_t buffer_size;
    size_t bytes_written;
} http_response_context_t;

/**
 * @brief HTTP 이벤트 핸들러 - 응답 데이터 수신
 */
static esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    http_response_context_t* ctx = (http_response_context_t*)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx->bytes_written + evt->data_len < ctx->buffer_size) {
                memcpy(ctx->buffer + ctx->bytes_written, evt->data, evt->data_len);
                ctx->bytes_written += evt->data_len;
                ctx->buffer[ctx->bytes_written] = '\0';
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief HTTP POST 요청 전송
 */
static esp_err_t http_post(const char* url, const char* request_body,
                           char* out_response, size_t response_size)
{
    T_LOGD(TAG, "post:%s", url);

    // HTTPS 요청 전 메모리 확보를 위해 가비지 컬렉션 유도
    heap_caps_malloc_extmem_enable(1024);  // 외부 메모리 허용
    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 대기로 메모리 안정화

    // 응답 컨텍스트 초기화
    http_response_context_t response_ctx = {
        .buffer = out_response,
        .buffer_size = response_size,
        .bytes_written = 0
    };
    out_response[0] = '\0';
    T_LOGD(TAG, "body:%s", request_body);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = LICENSE_HTTPS_TIMEOUT_MS;
    config.buffer_size = 4096;
    config.buffer_size_tx = 4096;
    config.user_agent = "ESP32-Tally-Node/1.0";
    config.keep_alive_enable = true;
    config.is_async = false;
    config.event_handler = http_event_handler;
    config.user_data = &response_ctx;
    // HTTPS/TLS 설정 - ISRG Root X1 인증서 사용
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = ISRG_ROOT_X1_CERT;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        T_LOGE(TAG, "fail:init");
        return ESP_FAIL;
    }

    // 헤더 설정
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-API-Key", LICENSE_API_KEY);

    // 바디 설정
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

    // 요청 전송
    T_LOGD(TAG, "sending...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);

        T_LOGD(TAG, "status:%d,len:%d", status, response_ctx.bytes_written);

        if (status != 200) {
            T_LOGE(TAG, "fail:http:%d", status);
            err = ESP_FAIL;
        }
    } else {
        // SSL/TLS 오류 상세 분류
        switch (err) {
            case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME:
                T_LOGE(TAG, "fail:dns_resolve");
                break;
            case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST:
                T_LOGE(TAG, "fail:connect_host");
                break;
            case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED:
                T_LOGE(TAG, "fail:ssl_handshake");
                break;
            case ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED:
                T_LOGE(TAG, "fail:cert_parse");
                break;
            default:
                T_LOGE(TAG, "fail:0x%x", err);
                break;
        }
    }

    esp_http_client_cleanup(client);
    return err;
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t license_client_init(void)
{
    T_LOGD(TAG, "init");
    return ESP_OK;
}

esp_err_t license_client_validate(const char* key, const char* mac_address,
                                  bool connected, license_validate_response_t* out_response)
{
    if (!key || !mac_address || !out_response) {
        return ESP_ERR_INVALID_ARG;
    }

    // 응답 구조체 초기화
    memset(out_response, 0, sizeof(license_validate_response_t));

    // WiFi 연결 확인 (license_service에서 확인 후 전달)
    if (!connected) {
        strncpy(out_response->error, "WiFi not connected", sizeof(out_response->error) - 1);
        T_LOGE(TAG, "fail:no_wifi");
        return ESP_ERR_INVALID_STATE;
    }

    // 요청 URL 생성
    char url[256];
    snprintf(url, sizeof(url), "%s%s", LICENSE_SERVER_BASE, LICENSE_VALIDATE_PATH);

    // JSON 요청 본문 생성
    cJSON* req_json = cJSON_CreateObject();
    cJSON_AddStringToObject(req_json, "license_key", key);
    cJSON_AddStringToObject(req_json, "mac_address", mac_address);

    char* request_body = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);

    T_LOGD(TAG, "validate:%s", key);

    // HTTP POST 전송
    char response_buffer[2048];
    esp_err_t err = http_post(url, request_body, response_buffer, sizeof(response_buffer));

    free(request_body);

    if (err != ESP_OK) {
        // SSL/TLS 오류에 따른 적절한 에러 메시지 제공
        switch (err) {
            case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME:
                strncpy(out_response->error, "DNS resolution failed", sizeof(out_response->error) - 1);
                break;
            case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST:
                strncpy(out_response->error, "Failed to connect to server", sizeof(out_response->error) - 1);
                break;
            case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED:
                strncpy(out_response->error, "SSL handshake failed", sizeof(out_response->error) - 1);
                break;
            case ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED:
                strncpy(out_response->error, "Certificate parse failed", sizeof(out_response->error) - 1);
                break;
            default:
                strncpy(out_response->error, "Server connection failed", sizeof(out_response->error) - 1);
                break;
        }
        return err;
    }

    T_LOGD(TAG, "resp:%s", response_buffer);

    // JSON 응답 파싱
    cJSON* res_json = cJSON_Parse(response_buffer);
    if (!res_json) {
        T_LOGE(TAG, "fail:json");
        strncpy(out_response->error, "JSON parsing failed", sizeof(out_response->error) - 1);
        return ESP_FAIL;
    }

    // success 필드 확인
    cJSON* success_json = cJSON_GetObjectItem(res_json, "success");
    if (success_json && cJSON_IsBool(success_json)) {
        out_response->success = cJSON_IsTrue(success_json);
    }

    if (out_response->success) {
        // 성공: device_limit 추출
        cJSON* license_json = cJSON_GetObjectItem(res_json, "license");
        if (license_json) {
            cJSON* limit_json = cJSON_GetObjectItem(license_json, "device_limit");
            if (limit_json && cJSON_IsNumber(limit_json)) {
                out_response->device_limit = (uint8_t)limit_json->valuedouble;
            }
        }
        T_LOGD(TAG, "ok");
    } else {
        // 실패: error 메시지 추출
        cJSON* error_json = cJSON_GetObjectItem(res_json, "error");
        if (error_json && cJSON_IsString(error_json)) {
            strncpy(out_response->error, error_json->valuestring,
                    sizeof(out_response->error) - 1);
        }
        T_LOGE(TAG, "fail:%s", out_response->error);
    }

    cJSON_Delete(res_json);
    return ESP_OK;
}

bool license_client_connection_test(void)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/api/connection-test", LICENSE_SERVER_BASE);

    T_LOGD(TAG, "conn_test:start:%s", url);

    // HTTPS 요청 전 메모리 확보를 위해 가비지 컬렉션 유도
    heap_caps_malloc_extmem_enable(1024);  // 외부 메모리 허용
    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 대기로 메모리 안정화

    T_LOGD(TAG, "conn_test:mem_ready");

    // 응답 버퍼
    char response_buffer[512] = {0};
    http_response_context_t response_ctx = {
        .buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .bytes_written = 0
    };

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = LICENSE_HTTPS_TIMEOUT_MS;
    config.user_agent = "ESP32-Tally-Node/1.0";
    config.event_handler = http_event_handler;
    config.user_data = &response_ctx;
    // HTTPS/TLS 설정 - ISRG Root X1 인증서 사용
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = ISRG_ROOT_X1_CERT;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;

    T_LOGD(TAG, "conn_test:init_client");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        T_LOGE(TAG, "conn_test:fail:init");
        return false;
    }
    T_LOGD(TAG, "conn_test:client_ok");

    esp_http_client_set_header(client, "X-API-Key", LICENSE_API_KEY);

    T_LOGD(TAG, "conn_test:performing...");
    esp_err_t err = esp_http_client_perform(client);
    T_LOGD(TAG, "conn_test:perform_done:0x%x", err);

    int status = esp_http_client_get_status_code(client);
    T_LOGD(TAG, "conn_test:status:%d", status);

    bool success = (err == ESP_OK && status == 200);

    // 연결 실패 시 서버 에러 메시지 파싱
    if (!success && response_ctx.bytes_written > 0) {
        T_LOGD(TAG, "conn_test:response:%s", response_buffer);
        cJSON* res_json = cJSON_Parse(response_buffer);
        if (res_json) {
            cJSON* error_json = cJSON_GetObjectItem(res_json, "error");
            if (error_json && cJSON_IsString(error_json)) {
                T_LOGE(TAG, "conn_test:error:%s", error_json->valuestring);
            }
            cJSON_Delete(res_json);
        }
    }

    esp_http_client_cleanup(client);

    if (success) {
        T_LOGD(TAG, "conn_test:ok");
    } else {
        T_LOGE(TAG, "conn_test:fail:err=0x%x,status=%d", err, status);
    }

    return success;
}

}  // extern "C"
