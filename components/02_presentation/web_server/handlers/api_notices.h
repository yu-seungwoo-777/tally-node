/**
 * @file api_notices.h
 * @brief API Notices 핸들러
 */

#ifndef TALLY_API_NOTICES_H
#define TALLY_API_NOTICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_http_client.h"

/**
 * @brief HTTP 응답 컨텍스트 구조체
 */
typedef struct {
    char* buffer;
    size_t buffer_size;
    size_t bytes_written;
} http_response_context_t;

/**
 * @brief esp_http_client 이벤트 핸들러
 * @param evt HTTP 클라이언트 이벤트
 * @return ESP_OK 항상 성공
 * @details esp_http_client의 데이터 수신 이벤트를 처리하여 버퍼에 저장합니다
 */
esp_err_t http_notices_event_handler(esp_http_client_event_t *evt);

/**
 * @brief GET /api/notices - 공지사항 조회 (duckdns 프록시)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_notices_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_NOTICES_H
