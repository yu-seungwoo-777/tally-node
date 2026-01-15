/**
 * @file web_server_helpers.h
 * @brief Web Server 공통 헬퍼 함수 (CORS, JSON 파싱/응답)
 */

#ifndef TALLY_WEB_SERVER_HELPERS_H
#define TALLY_WEB_SERVER_HELPERS_H

#include "esp_http_server.h"
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CORS 헤더
// ============================================================================

/**
 * @brief CORS 헤더 설정
 * @param req HTTP 요청 핸들러
 * @details Cross-Origin Resource Sharing 헤더를 설정하여 웹 브라우저에서의 API 접근을 허용합니다
 */
static inline void web_server_set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================================
// JSON 응답 헬퍼
// ============================================================================

/**
 * @brief JSON 응답 전송
 * @param req HTTP 요청 핸들러
 * @param json cJSON 객체 (전송 후 자동 해제됨)
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG json이 nullptr
 */
static inline esp_err_t web_server_send_json_response(httpd_req_t* req, cJSON* json)
{
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }

    char* json_str = cJSON_PrintUnformatted(json);
    if (!json_str) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * @brief JSON 에러 응답 전송
 * @param req HTTP 요청 핸들러
 * @param message 에러 메시지
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_error(httpd_req_t* req, const char* message)
{
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "error", message);
    }
    return web_server_send_json_response(req, json);
}

/**
 * @brief JSON 성공 응답 전송
 * @param req HTTP 요청 핸들러
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_ok(httpd_req_t* req)
{
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "status", "ok");
    }
    return web_server_send_json_response(req, json);
}

// ============================================================================
// HTTP 에러 응답 헬퍼 (표준화)
// ============================================================================

/**
 * @brief 400 Bad Request JSON 응답 전송
 * @param req HTTP 요청 핸들러
 * @param message 에러 메시지
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_bad_request(httpd_req_t* req, const char* message)
{
    httpd_resp_set_status(req, HTTPD_400);
    return web_server_send_json_error(req, message);
}

/**
 * @brief 404 Not Found JSON 응답 전송
 * @param req HTTP 요청 핸들러
 * @param message 에러 메시지
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_not_found(httpd_req_t* req, const char* message)
{
    httpd_resp_set_status(req, HTTPD_404);
    return web_server_send_json_error(req, message);
}

/**
 * @brief 500 Internal Server Error JSON 응답 전송
 * @param req HTTP 요청 핸들러
 * @param message 에러 메시지
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_internal_error(httpd_req_t* req, const char* message)
{
    httpd_resp_set_status(req, HTTPD_500);
    return web_server_send_json_error(req, message);
}

/**
 * @brief 413 Payload Too Large JSON 응답 전송
 * @param req HTTP 요청 핸들러
 * @param message 에러 메시지 (생략 가능)
 * @return ESP_OK
 */
static inline esp_err_t web_server_send_json_payload_too_large(httpd_req_t* req, const char* message)
{
    httpd_resp_set_status(req, "413 Payload Too Large");
    if (message && message[0] != '\0') {
        return web_server_send_json_error(req, message);
    }
    return web_server_send_json_error(req, "Payload too large");
}

// ============================================================================
// JSON 요청 파싱 헬퍼
// ============================================================================

/**
 * @brief 요청 크기 사전 검증
 * @param req HTTP 요청 핸들러
 * @param max_len 최대 허용 크기
 * @return true 크기가 허용 범위 내, false 초과
 * @details 초과 시 413 에러 응답을 자동으로 전송합니다
 */
static inline bool web_server_validate_content_length(httpd_req_t* req, size_t max_len)
{
    if (req->content_len > (int)max_len) {
        web_server_send_json_payload_too_large(req, "Request body too large");
        return false;
    }
    return true;
}

/**
 * @brief JSON 요청 바디 파싱
 * @param req HTTP 요청 핸들러
 * @param buf 읽기 버퍼
 * @param buf_len 버퍼 크기
 * @return cJSON 객체 (사용 후 cJSON_Delete로 해제), 실패 시 nullptr
 * @details 실패 시 HTTP 에러 응답을 자동으로 전송합니다
 */
static inline cJSON* web_server_parse_json_body(httpd_req_t* req, char* buf, size_t buf_len)
{
    // 요청 크기 사전 검증
    if (req->content_len >= (int)buf_len) {
        web_server_send_json_payload_too_large(req, nullptr);
        return nullptr;
    }

    int ret = httpd_req_recv(req, buf, buf_len - 1);
    if (ret <= 0) {
        web_server_send_json_bad_request(req, "Failed to read body");
        return nullptr;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        web_server_send_json_bad_request(req, "Invalid JSON");
    }
    return root;
}

// ============================================================================
// OPTIONS 핸들러 (공통)
// ============================================================================

/**
 * @brief CORS Preflight OPTIONS 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK
 */
static esp_err_t web_server_options_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_HELPERS_H
