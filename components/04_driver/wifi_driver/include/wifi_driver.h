/**
 * @file WiFiDriver.h
 * @brief WiFi Driver (WiFi HAL 래퍼)
 *
 * 역할: WiFi HAL을 사용하여 WiFi AP+STA 기능 제어
 * - AP 모드 제어
 * - STA 모드 제어
 * - 스캔 기능
 * - 상태 모니터링
 */

#ifndef WIFI_DRIVER_H
#define WIFI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi 상태
typedef struct {
    bool ap_started;
    bool sta_connected;
    char ap_ip[16];
    char sta_ip[16];
    int8_t sta_rssi;
    uint8_t ap_clients;
} wifi_driver_status_t;

// 네트워크 상태 변경 콜백
typedef void (*wifi_driver_status_callback_t)(bool connected, const char* ip);

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief WiFi Driver 초기화 (AP+STA)
 * @param ap_ssid AP SSID
 * @param ap_password AP 비밀번호 (NULL: 열린 네트워크)
 * @param sta_ssid STA SSID (NULL: 비활성화)
 * @param sta_password STA 비밀번호 (NULL: 열린 네트워크)
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_driver_init(const char* ap_ssid, const char* ap_password,
                           const char* sta_ssid, const char* sta_password);

/**
 * @brief WiFi Driver 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_driver_deinit(void);

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief WiFi 상태 조회
 * @return WiFi 상태 구조체
 */
wifi_driver_status_t wifi_driver_get_status(void);

/**
 * @brief 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool wifi_driver_is_initialized(void);

// ============================================================================
// STA 제어
// ============================================================================

/**
 * @brief STA 재연결
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_driver_sta_reconnect(void);

/**
 * @brief STA 연결 해제
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_driver_sta_disconnect(void);

/**
 * @brief STA 설정 변경 후 재연결 (AP는 유지)
 * @param ssid 새 SSID
 * @param password 새 비밀번호 (NULL: 열린 네트워크)
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_driver_sta_reconfig(const char* ssid, const char* password);

/**
 * @brief STA 연결 여부
 * @return true 연결됨, false 연결 안됨
 */
bool wifi_driver_sta_is_connected(void);

// ============================================================================
// AP 제어
// ============================================================================

/**
 * @brief AP 시작 여부
 * @return true 시작됨, false 시작 안됨
 */
bool wifi_driver_ap_is_started(void);

/**
 * @brief AP 연결 클라이언트 수
 * @return 클라이언트 수
 */
uint8_t wifi_driver_get_ap_clients(void);

// ============================================================================
// 콜백 설정
// ============================================================================

/**
 * @brief 상태 변경 콜백 설정
 * @param callback 콜백 함수
 */
void wifi_driver_set_status_callback(wifi_driver_status_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // WIFI_DRIVER_H
