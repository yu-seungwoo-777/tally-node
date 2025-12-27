/**
 * @file NetworkService.h
 * @brief 네트워크 통합 관리 서비스 (C++)
 *
 * 역할: WiFi와 Ethernet 통합 관리
 * - WiFi Driver 제어
 * - Ethernet Driver 제어
 * - 전체 네트워크 상태 관리
 * - 재시작 기능
 */

#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 네트워크 인터페이스 타입
typedef enum {
    NETWORK_IF_WIFI_AP = 0,
    NETWORK_IF_WIFI_STA,
    NETWORK_IF_ETHERNET,
    NETWORK_IF_MAX
} network_interface_t;

// 인터페이스 상태
typedef struct {
    bool active;
    bool connected;
    char ip[16];
    char netmask[16];
    char gateway[16];
} network_if_status_t;

// 전체 네트워크 상태
typedef struct {
    network_if_status_t wifi_ap;
    network_if_status_t wifi_sta;
    network_if_status_t ethernet;
} network_status_t;

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief Network Service 초기화 (설정 포함)
 * @param config 네트워크 설정 (WiFi, Ethernet)
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t network_service_init_with_config(const app_network_config_t* config);

/**
 * @brief Network Service 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t network_service_deinit(void);

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief 전체 네트워크 상태 조회
 * @return 전체 네트워크 상태 구조체
 */
network_status_t network_service_get_status(void);

/**
 * @brief 상태 로그 출력
 */
void network_service_print_status(void);

/**
 * @brief 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool network_service_is_initialized(void);

// ============================================================================
// 재시작 (설정 변경 후)
// ============================================================================

/**
 * @brief WiFi 재시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t network_service_restart_wifi(void);

/**
 * @brief Ethernet 재시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t network_service_restart_ethernet(void);

/**
 * @brief 전체 네트워크 재시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t network_service_restart_all(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_SERVICE_H
