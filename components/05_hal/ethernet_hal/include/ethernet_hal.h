/**
 * @file EthernetHal.h
 * @brief Ethernet HAL (W5500 SPI 하드웨어 제어 캡슐화)
 *
 * 역할: ESP-IDF esp_eth 드라이버 캡슐화
 * - W5500 SPI Ethernet 하드웨어 초기화/제어
 * - netif 생성/관리
 * - 이벤트 핸들링
 * - DHCP/Static IP 지원
 */

#ifndef ETHERNET_HAL_H
#define ETHERNET_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"  // for esp_netif_t

#ifdef __cplusplus
extern "C" {
#endif

// Ethernet 상태
typedef enum {
    ETHERNET_HAL_STATE_IDLE = 0,
    ETHERNET_HAL_STATE_STARTED,
    ETHERNET_HAL_STATE_STOPPED,
} ethernet_hal_state_t;

// Ethernet 상태 정보
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
} ethernet_hal_status_t;

// 이벤트 콜백 타입
typedef void (*ethernet_hal_event_callback_t)(void* arg, esp_event_base_t event_base,
                                              int32_t event_id, void* event_data);

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief Ethernet HAL 초기화
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_init(void);

/**
 * @brief Ethernet HAL 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_deinit(void);

// ============================================================================
// 제어
// ============================================================================

/**
 * @brief Ethernet 시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_start(void);

/**
 * @brief Ethernet 정지
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_stop(void);

/**
 * @brief Ethernet 재시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_restart(void);

// ============================================================================
// IP 설정
// ============================================================================

/**
 * @brief DHCP 모드 활성화
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_enable_dhcp(void);

/**
 * @brief Static IP 모드 활성화
 * @param ip IP 주소 (예: "192.168.0.100")
 * @param netmask 넷마스크 (예: "255.255.255.0")
 * @param gateway 게이트웨이 (예: "192.168.0.1")
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t ethernet_hal_enable_static(const char* ip, const char* netmask, const char* gateway);

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief Ethernet HAL 상태 조회
 * @return Ethernet HAL 상태
 */
ethernet_hal_state_t ethernet_hal_get_state(void);

/**
 * @brief Ethernet 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool ethernet_hal_is_initialized(void);

/**
 * @brief 링크 업 여부
 * @return true 링크 업, false 링크 다운
 */
bool ethernet_hal_is_link_up(void);

/**
 * @brief IP 할당 여부
 * @return true IP 할당됨, false 미할당
 */
bool ethernet_hal_has_ip(void);

/**
 * @brief 상태 정보 조회
 * @param status 상태 정보를 받을 구조체 포인터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t ethernet_hal_get_status(ethernet_hal_status_t* status);

/**
 * @brief netif 포인터 조회 (DNS 설정 등을 위해)
 * @return netif 포인터, 초기화되지 않은 경우 NULL
 */
esp_netif_t* ethernet_hal_get_netif(void);

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 이벤트 핸들러 등록
 * @param callback 이벤트 콜백 함수
 * @return ESP_OK 성공
 */
esp_err_t ethernet_hal_register_event_handler(ethernet_hal_event_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // ETHERNET_HAL_H
