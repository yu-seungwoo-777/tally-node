/**
 * @file ConfigService.h
 * @brief NVS 설정 관리 서비스 (C++)
 *
 * 역할: NVS(Non-Volatile Storage)를 사용한 설정 관리
 * - WiFi AP 설정
 * - WiFi STA 설정
 * - Ethernet 설정
 * - 설정 로드/저장
 * - 기본값 관리
 */

#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// C 구조체 (외부 인터페이스)
// ============================================================================

// WiFi AP 설정
typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    bool enabled;
} config_wifi_ap_t;

// WiFi STA 설정
typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} config_wifi_sta_t;

// Ethernet 설정
typedef struct {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
    bool enabled;
} config_ethernet_t;

// 전체 설정
typedef struct {
    config_wifi_ap_t wifi_ap;
    config_wifi_sta_t wifi_sta;
    config_ethernet_t ethernet;
} config_all_t;

// ============================================================================
// C 인터페이스
// ============================================================================

/**
 * @brief Config Service 초기화
 */
esp_err_t config_service_init(void);

/**
 * @brief 전체 설정 로드
 */
esp_err_t config_service_load_all(config_all_t* config);

/**
 * @brief 전체 설정 저장
 */
esp_err_t config_service_save_all(const config_all_t* config);

/**
 * @brief WiFi AP 설정 로드
 */
esp_err_t config_service_get_wifi_ap(config_wifi_ap_t* config);

/**
 * @brief WiFi AP 설정 저장
 */
esp_err_t config_service_set_wifi_ap(const config_wifi_ap_t* config);

/**
 * @brief WiFi STA 설정 로드
 */
esp_err_t config_service_get_wifi_sta(config_wifi_sta_t* config);

/**
 * @brief WiFi STA 설정 저장
 */
esp_err_t config_service_set_wifi_sta(const config_wifi_sta_t* config);

/**
 * @brief Ethernet 설정 로드
 */
esp_err_t config_service_get_ethernet(config_ethernet_t* config);

/**
 * @brief Ethernet 설정 저장
 */
esp_err_t config_service_set_ethernet(const config_ethernet_t* config);

/**
 * @brief 기본값 로드
 */
esp_err_t config_service_load_defaults(config_all_t* config);

/**
 * @brief 공장 초기화
 */
esp_err_t config_service_factory_reset(void);

/**
 * @brief 초기화 여부
 */
bool config_service_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_SERVICE_H
