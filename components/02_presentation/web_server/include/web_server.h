/**
 * @file web_server.h
 * @brief Web Server for Tally Node Control Interface (Event-based)
 */

#ifndef TALLY_WEB_SERVER_H
#define TALLY_WEB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <esp_err.h>

/**
 * @brief 웹 서버 초기화 (리소스 설정, URI 핸들러 등록)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_init(void);

/**
 * @brief 웹 서버 시작 (HTTP 서버 실행)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_start(void);

/**
 * @brief 웹 서버 중지
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_stop(void);

/**
 * @brief 웹 서버 상태 확인
 * @return true 실행 중, false 중지됨
 */
bool web_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_H
