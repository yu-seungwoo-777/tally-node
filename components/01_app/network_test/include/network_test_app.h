/**
 * @file network_test_app.h
 * @brief 네트워크 테스트 앱
 *
 * 01_app 계층 - 애플리케이션
 * - WiFi AP/STA 테스트
 * - Ethernet (W5500) 테스트
 * - NetworkService 사용하여 네트워크 제어
 * - ConfigService 사용하여 설정 관리
 */

#ifndef NETWORK_TEST_APP_H
#define NETWORK_TEST_APP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 네트워크 테스트 앱 초기화
 *
 * @return ESP_OK 성공
 */
esp_err_t network_test_app_init(void);

/**
 * @brief 네트워크 테스트 앱 시작
 *
 * @return ESP_OK 성공
 */
esp_err_t network_test_app_start(void);

/**
 * @brief 네트워크 테스트 앱 정지
 */
void network_test_app_stop(void);

/**
 * @brief 네트워크 테스트 앱 해제
 */
void network_test_app_deinit(void);

/**
 * @brief 실행 중 여부 확인
 *
 * @return true 실행 중
 */
bool network_test_app_is_running(void);

/**
 * @brief WiFi 스캔 실행
 *
 * @return ESP_OK 성공
 */
esp_err_t network_test_app_wifi_scan(void);

/**
 * @brief WiFi STA 재연결
 *
 * @return ESP_OK 성공
 */
esp_err_t network_test_app_wifi_sta_reconnect(void);

/**
 * @brief Ethernet 재시작
 *
 * @return ESP_OK 성공
 */
esp_err_t network_test_app_ethernet_restart(void);

/**
 * @brief 전체 상태 출력
 */
void network_test_app_print_status(void);

/**
 * @brief 설정 출력
 */
void network_test_app_print_config(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_TEST_APP_H
