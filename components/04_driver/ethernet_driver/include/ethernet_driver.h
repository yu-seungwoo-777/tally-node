/**
 * @file EthernetDriver.h
 * @brief Ethernet Driver (Ethernet HAL 래퍼)
 *
 * 역할: Ethernet HAL을 사용하여 W5500 Ethernet 제어
 * - W5500 Ethernet 제어
 * - DHCP/Static IP 모드 전환
 * - DHCP 폴백 (10초 타임아웃)
 * - 상태 모니터링
 */

#ifndef ETHERNET_DRIVER_H
#define ETHERNET_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ethernet 상태
typedef struct {
    bool initialized;
    bool detected;       // W5500 칩 감지 여부
    bool link_up;
    bool got_ip;
    bool dhcp_mode;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char mac[18];
} ethernet_driver_status_t;

// 상태 변경 콜백
typedef void (*ethernet_driver_status_callback_t)(void);

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief Ethernet Driver 초기화
 * @param dhcp_enabled DHCP 모드 활성화
 * @param static_ip Static IP 주소 (DHCP 모드 시 폴백용)
 * @param static_netmask Static 넷마스크
 * @param static_gateway Static 게이트웨이
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_driver_init(bool dhcp_enabled,
                               const char* static_ip,
                               const char* static_netmask,
                               const char* static_gateway);

/**
 * @brief Ethernet Driver 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_driver_deinit(void);

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief Ethernet 상태 조회
 * @return Ethernet 상태 구조체
 */
ethernet_driver_status_t ethernet_driver_get_status(void);

/**
 * @brief 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool ethernet_driver_is_initialized(void);

/**
 * @brief 링크 업 여부
 * @return true 링크 업, false 링크 다운
 */
bool ethernet_driver_is_link_up(void);

/**
 * @brief IP 할당 여부
 * @return true IP 할당됨, false 미할당
 */
bool ethernet_driver_has_ip(void);

// ============================================================================
// IP 모드 변경
// ============================================================================

/**
 * @brief DHCP 모드 활성화
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_driver_enable_dhcp(void);

/**
 * @brief Static IP 모드 활성화
 * @param ip IP 주소
 * @param netmask 넷마스크
 * @param gateway 게이트웨이
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_driver_enable_static(const char* ip, const char* netmask, const char* gateway);

// ============================================================================
// 제어
// ============================================================================

/**
 * @brief Ethernet 재시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_driver_restart(void);

// ============================================================================
// 콜백 설정
// ============================================================================

/**
 * @brief 상태 변경 콜백 설정
 * @param callback 콜백 함수
 */
void ethernet_driver_set_status_callback(ethernet_driver_status_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_DRIVER_H
