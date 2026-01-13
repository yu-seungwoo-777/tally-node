/**
 * @file api_led.h
 * @brief API LED 핸들러
 */

#ifndef TALLY_API_LED_H
#define TALLY_API_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief GET /api/led/colors - LED 색상 조회 (캐시 또는 요청 이벤트)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_led_colors_get_handler(httpd_req_t* req);

/**
 * @brief POST /api/led/colors - LED 색상 설정
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_led_colors_post_handler(httpd_req_t* req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_LED_H
