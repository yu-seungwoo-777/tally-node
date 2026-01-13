/**
 * @file api_test.h
 * @brief API Test 핸들러
 */

#ifndef TALLY_API_TEST_H
#define TALLY_API_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief POST /api/test/start - 테스트 모드 시작
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_test_start_handler(httpd_req_t* req);

/**
 * @brief POST /api/test/stop - 테스트 모드 중지
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_test_stop_handler(httpd_req_t* req);

/**
 * @brief POST /api/test/internet - 인터넷 연결 테스트 (8.8.8.8 핑)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_test_internet_handler(httpd_req_t* req);

/**
 * @brief POST /api/test/license-server - 라이센스 서버 연결 테스트 (프록시 통해)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_test_license_server_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_TEST_H
