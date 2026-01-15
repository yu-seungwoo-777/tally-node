/**
 * @file api_license.cpp
 * @brief API License 핸들러 구현
 */

#include "api_license.h"
#include "web_server_helpers.h"
#include "event_bus.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "02_WebSvr_License";

extern "C" {

esp_err_t api_license_validate_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[512];
    int ret = httpd_req_recv(req, buf, 511);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 라이센스 키 추출
    cJSON* key_json = cJSON_GetObjectItem(root, "key");
    if (!key_json || !cJSON_IsString(key_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'key' field");
        return ESP_FAIL;
    }

    const char* key = key_json->valuestring;
    if (strlen(key) != 16) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid key length\"}");
        return ESP_OK;
    }

    // 라이센스 검증 이벤트 발행
    license_validate_event_t validate_req;
    strncpy(validate_req.key, key, 16);
    validate_req.key[16] = '\0';
    event_bus_publish(EVT_LICENSE_VALIDATE, &validate_req, sizeof(validate_req));

    cJSON_Delete(root);

    // 응답 (검증은 비동기로 처리됨, 상태는 EVT_LICENSE_STATE_CHANGED로 업데이트됨)
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"accepted\"}");

    return ESP_OK;
}

} // extern "C"
