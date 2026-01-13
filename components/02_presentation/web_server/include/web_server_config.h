/**
 * @file web_server_config.h
 * @brief Web Server 설정 파싱 함수 모듈
 */

#ifndef TALLY_WEB_SERVER_CONFIG_H
#define TALLY_WEB_SERVER_CONFIG_H

#include "esp_err.h"
#include "cJSON.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 스위처 설정 파싱 함수
// ============================================================================

/**
 * @brief Switcher 공통 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 */
void web_server_config_parse_switcher_common_fields(cJSON* root, config_save_request_t* save_req);

/**
 * @brief 스위처 Primary 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_switcher_primary(cJSON* root, config_save_request_t* save_req);

/**
 * @brief 스위처 Secondary 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_switcher_secondary(cJSON* root, config_save_request_t* save_req);

/**
 * @brief 스위처 Dual 모드 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_switcher_dual(cJSON* root, config_save_request_t* save_req);

// ============================================================================
// 네트워크 설정 파싱 함수
// ============================================================================

/**
 * @brief 네트워크 AP 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_network_ap(cJSON* root, config_save_request_t* save_req);

/**
 * @brief 네트워크 WiFi STA 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_network_wifi(cJSON* root, config_save_request_t* save_req);

/**
 * @brief 네트워크 Ethernet 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 * @return ESP_OK 성공
 */
esp_err_t web_server_config_parse_network_ethernet(cJSON* root, config_save_request_t* save_req);

// ============================================================================
// 네트워크 재시작 이벤트 발행 함수
// ============================================================================

/**
 * @brief 네트워크 재시작 이벤트 발행
 * @param save_req 설정 저장 요청 구조체
 */
void web_server_config_publish_network_restart(const config_save_request_t* save_req);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_CONFIG_H
