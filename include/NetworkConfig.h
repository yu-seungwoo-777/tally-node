/**
 * @file NetworkConfig.h
 * @brief 네트워크 (WiFi/Ethernet) 기본 설정값
 *
 * @note 이 파일은 WiFi 및 Ethernet 연결에 대한 설정입니다.
 *
 * @section 설정 항목
 *   - WiFi AP/STA 모드 설정
 *   - Ethernet (W5500) SPI 및 타임아웃 설정
 *   - DHCP/Static IP 설정
 *   - 네트워크 연결 타임아웃 (WiFi/Ethernet 공통)
 *   - NVS 키 이름 (설정 저장용)
 */

#pragma once

#include <stdint.h>

// ============================================================================
// 네트워크 인터페이스 타입
// ============================================================================

#define NETWORK_INTERFACE_WIFI      1    ///< WiFi 사용
#define NETWORK_INTERFACE_ETHERNET   2    ///< Ethernet 사용

// ============================================================================
// WiFi 기본 설정
// ============================================================================

// AP 모드 설정
#define WIFI_AP_SSID              "TallyNode_AP"        ///< AP 모드 SSID
#define WIFI_AP_PASSWORD          ""                  ///< AP 모드 비밀번호 (빈값=오픈)
#define WIFI_AP_CHANNEL           0                   ///< AP 채널 (0=자동, 1~13=고정)
#define WIFI_AP_MAX_CONN          4                   ///< 최대 연결 수
#define WIFI_AP_IP                "192.168.4.1"       ///< AP 모드 IP 주소
#define WIFI_AP_GATEWAY           "192.168.4.1"       ///< AP 모드 게이트웨이
#define WIFI_AP_NETMASK           "255.255.255.0"     ///< AP 모드 넷마스크

// STA 모드 설정
#define WIFI_STA_SSID             "HOME WIFI"         ///< STA 모드 SSID (설정 필요)
#define WIFI_STA_PASSWORD         "33333333"          ///< STA 모드 비밀번호

// WiFi 스캔 설정
#define WIFI_SCAN_TIMEOUT_MS      10000               ///< 스캔 타임아웃 (10초)
#define WIFI_SCAN_CHANNEL_TIME    200                 ///< 채널당 스캔 시간 (ms)

// ============================================================================
// Ethernet (W5500) 기본 설정
// ============================================================================

// SPI 설정
#define W5500_SPI_CLOCK          20 * 1000 * 1000    ///< 20MHz
#define W5500_SPI_MODE           0                   ///< SPI 모드 0
#define W5500_POLL_PERIOD_MS     100                 ///< 폴링 간격 (INT 핀 미사용시)

// 타임아웃 설정
#define W5500_RESET_DELAY_MS     10                  ///< 리셋 LOW 유지 시간
#define W5500_RESET_WAIT_MS      50                  ///< 리셋 후 안정화 시간
#define W5500_LINK_CHECK_MS      2000                ///< 링크 체크 간격

// ============================================================================
// DHCP 설정
// ============================================================================

#define DHCP_ENABLED             1                   ///< DHCP 사용 (1=사용, 0=미사용)

// Static IP 설정 (DHCP 미사용시)
#define STATIC_IP                "192.168.0.100"     ///< Static IP 주소
#define STATIC_NETMASK           "255.255.255.0"     ///< 넷마스크
#define STATIC_GATEWAY           "192.168.0.1"       ///< 게이트웨이
#define STATIC_DNS               "192.168.0.1"       ///< DNS 서버

// ============================================================================
// 공통 네트워크 설정
// ============================================================================

#define NETWORK_CONNECT_TIMEOUT_MS   10000    ///< 네트워크 연결 타임아웃 (10초)
#define NETWORK_RETRY_INTERVAL_MS    5000     ///< 재연결 시도 간격 (5초)

// MTU 설정
#define NETWORK_MTU             1500                ///< 최대 전송 단위 (바이트)

// ============================================================================
// NVS 키 이름 (설정 저장용)
// ============================================================================

// WiFi 설정
#define NVS_WIFI_ENABLED         "wifi_enabled"       ///< WiFi 사용 여부
#define NVS_WIFI_SSID            "wifi_ssid"          ///< WiFi SSID
#define NVS_WIFI_PASSWORD        "wifi_password"      ///< WiFi 비밀번호
#define NVS_WIFI_CHANNEL         "wifi_channel"       ///< WiFi 채널 (AP 모드)

// Ethernet 설정
#define NVS_ETH_ENABLED          "eth_enabled"        ///< Ethernet 사용 여부
#define NVS_ETH_USE_DHCP         "eth_dhcp"           ///< DHCP 사용 여부
#define NVS_ETH_IP               "eth_ip"             ///< Static IP
#define NVS_ETH_NETMASK          "eth_netmask"        ///< 넷마스크
#define NVS_ETH_GATEWAY          "eth_gateway"        ///< 게이트웨이

// 네트워크 우선순위
#define NVS_NET_PRIORITY         "net_priority"       ///< 네트워크 우선순위 (1=WiFi, 2=Ethernet)

// ============================================================================
// 유틸리티 매크로
// ============================================================================

/**
 * @brief NVS에 WiFi 설정이 저장되어 있는지 확인
 */
#define WIFI_CONFIG_SAVED()  (NVS_WIFI_SSID != nullptr && strlen(NVS_WIFI_SSID) > 0)

/**
 * @brief 네트워크 설정가 유효한지 확인
 */
#define IS_VALID_IP(ip)  ((ip) != nullptr && strlen(ip) > 0 && (ip)[0] != '\0')
