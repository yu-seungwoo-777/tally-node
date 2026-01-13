/**
 * @file api_config.h
 * @brief API Config 핸들러
 */

#ifndef TALLY_API_CONFIG_H
#define TALLY_API_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief POST /api/config/path - 설정 저장 (이벤트 기반)
 * @param req HTTP 요청
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t api_config_post_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_CONFIG_H
