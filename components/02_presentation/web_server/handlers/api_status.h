/**
 * @file api_status.h
 * @brief API Status 핸들러
 */

#ifndef TALLY_API_STATUS_H
#define TALLY_API_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief GET /api/status - 전체 상태 반환 (캐시 데이터 사용)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_status_handler(httpd_req_t* req);

/**
 * @brief POST /api/reboot - 시스템 재부팅
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_reboot_handler(httpd_req_t* req);

/**
 * @brief POST /api/reboot/broadcast - 전체 디바이스 재부팅 (브로드캐스트 + TX 재부팅)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_reboot_broadcast_handler(httpd_req_t* req);

/**
 * @brief POST /api/factory-reset - 공장 초기화 (NVS 초기화 후 재부팅)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_factory_reset_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_STATUS_H
