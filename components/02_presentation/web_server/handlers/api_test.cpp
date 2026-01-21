/**
 * @file api_test.cpp
 * @brief API Test 핸들러 구현
 */

#include "api_test.h"
#include "web_server_helpers.h"
#include "event_bus.h"
#include "t_log.h"
#include "esp_timer.h"
#include "sys/socket.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "02_WS_Test";

// 연결 테스트 결과 저장 (이벤트 핸들러에서 설정)
static bool s_connection_test_result = false;
static bool s_connection_test_done = false;
static char s_connection_test_error[128] = {0};  // 에러 메시지 저장

extern "C" {

esp_err_t api_test_start_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 크기 사전 검증
    if (!web_server_validate_content_length(req, 128)) {
        return ESP_FAIL;
    }

    // JSON 파싱
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        T_LOGE(TAG, "httpd_req_recv failed: ret=%d", ret);
        return web_server_send_json_bad_request(req, "Invalid request");
    }
    buf[ret] = '\0';
    T_LOGD(TAG, "Received JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        T_LOGE(TAG, "cJSON_Parse failed");
        return web_server_send_json_bad_request(req, "Invalid JSON");
    }

    // 파라미터 추출
    cJSON* max_channels_item = cJSON_GetObjectItem(root, "max_channels");
    cJSON* interval_ms_item = cJSON_GetObjectItem(root, "interval_ms");

    if (!max_channels_item || !interval_ms_item) {
        T_LOGE(TAG, "Missing parameters: max_channels=%p, interval_ms=%p",
               max_channels_item, interval_ms_item);
        cJSON_Delete(root);
        return web_server_send_json_bad_request(req, "Missing parameters");
    }

    uint8_t max_channels = (uint8_t)cJSON_GetNumberValue(max_channels_item);
    uint16_t interval_ms = (uint16_t)cJSON_GetNumberValue(interval_ms_item);
    cJSON_Delete(root);

    T_LOGD(TAG, "Parsed params: max_channels=%d, interval_ms=%d", max_channels, interval_ms);

    // 파라미터 검증
    if (max_channels < 1 || max_channels > 20) {
        T_LOGE(TAG, "Invalid max_channels: %d", max_channels);
        return web_server_send_json_bad_request(req, "max_channels must be 1-20");
    }

    if (interval_ms < 100 || interval_ms > 3000) {
        T_LOGE(TAG, "Invalid interval_ms: %d", interval_ms);
        return web_server_send_json_bad_request(req, "interval_ms must be 100-3000");
    }

    // 이벤트 발행
    tally_test_mode_config_t test_config = {
        .max_channels = max_channels,
        .interval_ms = interval_ms
    };
    event_bus_publish(EVT_TALLY_TEST_MODE_START, &test_config, sizeof(test_config));

    return web_server_send_json_ok(req);
}

esp_err_t api_test_stop_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 이벤트 발행
    event_bus_publish(EVT_TALLY_TEST_MODE_STOP, nullptr, 0);

    return web_server_send_json_ok(req);
}

esp_err_t api_test_internet_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return web_server_send_json_internal_error(req, "Memory allocation failed");
    }

    bool success = false;
    int ping_ms = 0;

    // 8.8.8.8 (Google DNS) 핑 테스트
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);  // DNS 포트
        inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

        // 타이머 시작
        int64_t start = esp_timer_get_time();

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            int64_t end = esp_timer_get_time();
            ping_ms = (end - start) / 1000;  // ms 변환
            success = true;
        }

        close(sock);
    }

    cJSON_AddBoolToObject(root, "success", success);
    if (success) {
        cJSON_AddNumberToObject(root, "ping", ping_ms);
    }

    return web_server_send_json_response(req, root);
}

// 연결 테스트 결과 이벤트 핸들러
static esp_err_t on_connection_test_result(const event_data_t* event)
{
    if (event->type != EVT_LICENSE_CONNECTION_TEST_RESULT) {
        return ESP_OK;
    }

    const license_connection_test_result_t* result =
        (const license_connection_test_result_t*)event->data;

    s_connection_test_result = result->success;
    s_connection_test_done = true;

    // 에러 메시지 캡처
    if (!result->success && result->error[0] != '\0') {
        strncpy(s_connection_test_error, result->error, sizeof(s_connection_test_error) - 1);
        s_connection_test_error[sizeof(s_connection_test_error) - 1] = '\0';
    } else {
        s_connection_test_error[0] = '\0';
    }

    T_LOGD(TAG, "connection test result: success=%d, error=%s",
           s_connection_test_result, s_connection_test_error);

    return ESP_OK;
}

esp_err_t api_test_license_server_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 결과 플래그 초기화
    s_connection_test_done = false;
    s_connection_test_result = false;
    s_connection_test_error[0] = '\0';

    // 결과 이벤트 임시 구독
    event_bus_subscribe(EVT_LICENSE_CONNECTION_TEST_RESULT, on_connection_test_result);

    // 연결 테스트 요청 이벤트 발행
    event_bus_publish(EVT_LICENSE_CONNECTION_TEST, NULL, 0);

    // 결과 대기 (최대 10초)
    int retry = 100;  // 100ms * 100 = 10초
    while (!s_connection_test_done && retry-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 구독 해제
    event_bus_unsubscribe(EVT_LICENSE_CONNECTION_TEST_RESULT, on_connection_test_result);

    // JSON 응답 생성
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return web_server_send_json_internal_error(req, "Memory allocation failed");
    }

    cJSON_AddBoolToObject(root, "success", s_connection_test_result);
    if (!s_connection_test_result && s_connection_test_error[0] != '\0') {
        cJSON_AddStringToObject(root, "error", s_connection_test_error);
    }

    return web_server_send_json_response(req, root);
}

} // extern "C"
