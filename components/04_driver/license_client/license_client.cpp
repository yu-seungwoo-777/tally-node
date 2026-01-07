/**
 * @file license_client.cpp
 * @brief 라이센스 서버 HTTP 클라이언트 구현
 */

#include "license_client.h"
#include "t_log.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "LicenseClient";

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
    T_LOGI(TAG, "HTTP 요청 시작: %s", url);

    // 응답 컨텍스트 초기화
    http_response_context_t response_ctx = {
        .buffer = out_response,
        .buffer_size = response_size,
        .bytes_written = 0
    };
    out_response[0] = '\0';
    T_LOGI(TAG, "요청 바디: %s", request_body);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = LICENSE_TIMEOUT_MS;
    config.buffer_size = 4096;
    config.buffer_size_tx = 4096;
    config.user_agent = "ESP32-Tally-Node/1.0";
    config.cert_pem = nullptr;  // HTTP 연결 (HTTPS 아님)
    config.keep_alive_enable = true;
    config.is_async = false;
    config.event_handler = http_event_handler;
    config.user_data = &response_ctx;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        T_LOGE(TAG, "HTTP 클라이언트 초기화 실패");
        return ESP_FAIL;
    }

    // 헤더 설정
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-API-Key", LICENSE_API_KEY);
    T_LOGI(TAG, "헤더 설정: X-API-Key: %s", LICENSE_API_KEY);

    // 바디 설정
    esp_http_client_set_post_field(client, request_body, strlen(request_body));

    // 요청 전송
    T_LOGI(TAG, "esp_http_client_perform() 호출...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);

        T_LOGI(TAG, "HTTP POST Status: %d, 응답 길이: %d", status, response_ctx.bytes_written);

        if (status != 200) {
            T_LOGE(TAG, "HTTP 에러: %d", status);
            err = ESP_FAIL;
        }
    } else {
        T_LOGE(TAG, "HTTP 요청 실패: %s (0x%x)", esp_err_to_name(err), err);
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
    T_LOGI(TAG, "LicenseClient 초기화");
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
        strncpy(out_response->error, "WiFi 연결 안됨", sizeof(out_response->error) - 1);
        T_LOGE(TAG, "WiFi 연결 안됨 (license_service에서 확인)");
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

    T_LOGI(TAG, "라이센스 검증 요청: %s", key);

    // HTTP POST 전송
    char response_buffer[2048];
    esp_err_t err = http_post(url, request_body, response_buffer, sizeof(response_buffer));

    free(request_body);

    if (err != ESP_OK) {
        strncpy(out_response->error, "서버 연결 실패", sizeof(out_response->error) - 1);
        return err;
    }

    T_LOGI(TAG, "서버 응답: %s", response_buffer);

    // JSON 응답 파싱
    cJSON* res_json = cJSON_Parse(response_buffer);
    if (!res_json) {
        T_LOGE(TAG, "JSON 파싱 실패");
        strncpy(out_response->error, "JSON 파싱 실패", sizeof(out_response->error) - 1);
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
                T_LOGI(TAG, "라이센스 검증 성공: device_limit = %d", out_response->device_limit);
            }
        }
    } else {
        // 실패: error 메시지 추출
        cJSON* error_json = cJSON_GetObjectItem(res_json, "error");
        if (error_json && cJSON_IsString(error_json)) {
            strncpy(out_response->error, error_json->valuestring,
                    sizeof(out_response->error) - 1);
        }
        T_LOGE(TAG, "라이센스 검증 실패: %s", out_response->error);
    }

    cJSON_Delete(res_json);
    return ESP_OK;
}

bool license_client_connection_test(void)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/api/connection-test", LICENSE_SERVER_BASE);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = LICENSE_TIMEOUT_MS;
    config.user_agent = "ESP32-Tally-Node/1.0";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-API-Key", LICENSE_API_KEY);

    esp_err_t err = esp_http_client_perform(client);
    bool success = (err == ESP_OK && esp_http_client_get_status_code(client) == 200);

    esp_http_client_cleanup(client);

    if (success) {
        T_LOGI(TAG, "서버 연결 테스트 성공");
    } else {
        T_LOGE(TAG, "서버 연결 테스트 실패");
    }

    return success;
}

}  // extern "C"
