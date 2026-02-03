/**
 * @file api_static.h
 * @brief API Static 핸들러 (정적 파일)
 */

#ifndef TALLY_API_STATIC_H
#define TALLY_API_STATIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 인덱스 HTML 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 항상 성공
 * @details 임베디드된 index.html 파일을 응답으로 전송합니다
 */
esp_err_t index_handler(httpd_req_t* req);

/**
 * @brief CSS 파일 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 항상 성공
 * @details 임베디드된 styles.css 파일을 응답으로 전송합니다
 */
esp_err_t css_handler(httpd_req_t* req);

/**
 * @brief JS 파일 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 항상 성공
 * @details 임베디드된 app.bundle.js 파일을 응답으로 전송합니다
 */
esp_err_t js_handler(httpd_req_t* req);

/**
 * @brief Favicon 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 항상 성공
 * @details 빈 응답(204 No Content)을 반환하여 404 에러를 방지합니다
 */
esp_err_t favicon_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_STATIC_H
