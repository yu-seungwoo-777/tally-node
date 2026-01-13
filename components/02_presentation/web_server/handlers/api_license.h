/**
 * @file api_license.h
 * @brief API License 핸들러
 */

#ifndef TALLY_API_LICENSE_H
#define TALLY_API_LICENSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief POST /api/license/validate - 라이센스 키 검증 (이벤트 기반)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_license_validate_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_LICENSE_H
