/**
 * @file app_types.h
 * @brief 앱/서비스 간 공유 타입 정의
 *
 * event_bus를 통하지 않고 직접 호출하는 경우 사용하는 공통 구조체
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 펌웨어 버전 (PlatformIO build_flags에서 정의: -DFIRMWARE_VERSION=\"x.y.z\")
// app_types.h에서는 PlatformIO 정의가 없을 때만 기본값 제공
// ============================================================================

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "2.0.1"  // platformio.ini에서 관리하는 것이 권장됨
#endif

// ============================================================================
// 네트워크 설정 타입
// ============================================================================

/**
 * @brief WiFi AP 설정
 */
typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    bool enabled;
} app_wifi_ap_t;

/**
 * @brief WiFi STA 설정
 */
typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} app_wifi_sta_t;

/**
 * @brief Ethernet 설정
 */
typedef struct {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
    bool enabled;
} app_ethernet_t;

/**
 * @brief 네트워크 전체 설정
 */
typedef struct {
    app_wifi_ap_t wifi_ap;
    app_wifi_sta_t wifi_sta;
    app_ethernet_t ethernet;
} app_network_config_t;

#ifdef __cplusplus
}
#endif

#endif // APP_TYPES_H
