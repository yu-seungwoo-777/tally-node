/**
 * @file network_config_types.h
 * @brief 네트워크 설정 공통 타입 (config_service, network_service 공유)
 */

#ifndef NETWORK_CONFIG_TYPES_H
#define NETWORK_CONFIG_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi AP 설정
typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    bool enabled;
} network_config_wifi_ap_t;

// WiFi STA 설정
typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} network_config_wifi_sta_t;

// Ethernet 설정
typedef struct {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
    bool enabled;
} network_config_ethernet_t;

// 네트워크 전체 설정
typedef struct {
    network_config_wifi_ap_t wifi_ap;
    network_config_wifi_sta_t wifi_sta;
    network_config_ethernet_t ethernet;
} network_config_all_t;

#ifdef __cplusplus
}
#endif

#endif // NETWORK_CONFIG_TYPES_H
