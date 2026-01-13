/**
 * @file api_lora.h
 * @brief API LoRa 핸들러
 */

#ifndef TALLY_API_LORA_H
#define TALLY_API_LORA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief GET /api/lora/scan - 스캔 상태 및 결과 반환
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_lora_scan_get_handler(httpd_req_t* req);

/**
 * @brief POST /api/lora/scan/start - 스캔 시작
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_lora_scan_start_handler(httpd_req_t* req);

/**
 * @brief POST /api/lora/scan/stop - 스캔 중지
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_lora_scan_stop_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_LORA_H
