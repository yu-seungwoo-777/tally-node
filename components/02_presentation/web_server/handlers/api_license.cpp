/**
 * @file api_license.cpp
 * @brief API License 핸들러 구현
 */

#include "api_license.h"
#include "web_server_helpers.h"
#include "event_bus.h"
#include "cJSON.h"
#include <cstring>

extern "C" {

esp_err_t api_license_validate_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 크기 사전 검증
    if (!web_server_validate_content_length(req, 512)) {
        return ESP_FAIL;
    }

    // 요청 바디 읽기 (스택 할당)
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return web_server_send_json_bad_request(req, "Failed to read body");
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return web_server_send_json_bad_request(req, "Invalid JSON");
    }

    // 라이센스 키 추출
    cJSON* key_json = cJSON_GetObjectItem(root, "key");
    if (!key_json || !cJSON_IsString(key_json)) {
        cJSON_Delete(root);
        return web_server_send_json_bad_request(req, "Missing 'key' field");
    }

    const char* key = key_json->valuestring;
    if (strlen(key) != 16) {
        cJSON_Delete(root);
        // 키 길이 오류는 200으로 응답하되 에러 메시지 포함
        cJSON* json = cJSON_CreateObject();
        if (json) {
            cJSON_AddStringToObject(json, "status", "error");
            cJSON_AddStringToObject(json, "message", "Invalid key length");
        }
        return web_server_send_json_response(req, json);
    }

    // 라이센스 검증 이벤트 발행
    license_validate_event_t validate_req;
    strncpy(validate_req.key, key, 16);
    validate_req.key[16] = '\0';
    event_bus_publish(EVT_LICENSE_VALIDATE, &validate_req, sizeof(validate_req));

    cJSON_Delete(root);

    // 응답 (검증은 비동기로 처리됨, 상태는 EVT_LICENSE_STATE_CHANGED로 업데이트됨)
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "status", "accepted");
    }
    return web_server_send_json_response(req, json);
}

} // extern "C"
