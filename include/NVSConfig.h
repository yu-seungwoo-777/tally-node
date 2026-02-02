/**
 * @file NVSConfig.h
 * @brief NVS 설정 기본값 (ConfigService에서 사용)
 *
 * @note ConfigService의 하드코딩된 기본값을 중앙 관리합니다.
 */

#pragma once

#include <stdint.h>

// ============================================================================
// WiFi AP 기본값
// ============================================================================

#define NVS_WIFI_AP_SSID              "TallyNode_AP"
#define NVS_WIFI_AP_PASSWORD          "12345678"
#define NVS_WIFI_AP_CHANNEL           1

// ============================================================================
// WiFi STA 기본값
// ============================================================================

#define NVS_WIFI_STA_SSID             ""
#define NVS_WIFI_STA_PASSWORD         ""

// ============================================================================
// Ethernet 기본값
// ============================================================================

#define NVS_ETHERNET_DHCP_ENABLED     1
#define NVS_ETHERNET_STATIC_IP        "192.168.10.100"
#define NVS_ETHERNET_STATIC_NETMASK   "255.255.255.0"
#define NVS_ETHERNET_STATIC_GATEWAY   "192.168.10.1"

// ============================================================================
// LoRa 기본값
// ============================================================================

// 주파수 대역별 기본값 (SX1262: 868MHz, SX1268: 433MHz)
#define NVS_LORA_DEFAULT_FREQ_868     868.0f
#define NVS_LORA_DEFAULT_FREQ_433     433.0f

// 공통 RF 설정
#define NVS_LORA_DEFAULT_SYNC_WORD    0x12
#define NVS_LORA_DEFAULT_SF           7         // SF7
#define NVS_LORA_DEFAULT_CR           7         // CR 4/7
#define NVS_LORA_DEFAULT_BW           250.0f    // 250kHz
#define NVS_LORA_DEFAULT_TX_POWER     22        // 22dBm

// ============================================================================
// Switcher 기본값
// ============================================================================

// Primary Switcher
#define NVS_SWITCHER_PRI_TYPE         0         // 0=ATEM, 1=OBS, 2=vMix
#define NVS_SWITCHER_PRI_IP           "192.168.10.240"
#define NVS_SWITCHER_PRI_PORT         0         // 0=기본 포트 사용
#define NVS_SWITCHER_PRI_PASSWORD     ""
#define NVS_SWITCHER_PRI_INTERFACE    0         // 1=WiFi, 2=Ethernet
#define NVS_SWITCHER_PRI_CAMERA_LIMIT 0         // 0=무제한
#define NVS_SWITCHER_PRI_DEBUG_PACKET false     // 패킷 디버그 로그

// Secondary Switcher
#define NVS_SWITCHER_SEC_TYPE         0         // 0=ATEM
#define NVS_SWITCHER_SEC_IP           "192.168.10.244"
#define NVS_SWITCHER_SEC_PORT         0
#define NVS_SWITCHER_SEC_PASSWORD     ""
#define NVS_SWITCHER_SEC_INTERFACE    0         // WiFi
#define NVS_SWITCHER_SEC_CAMERA_LIMIT 0
#define NVS_SWITCHER_SEC_DEBUG_PACKET false     // 패킷 디버그 로그

// Dual Mode
#define NVS_DUAL_ENABLED              false
#define NVS_DUAL_OFFSET               5

// ============================================================================
// Device 기본값
// ============================================================================

#define NVS_DEVICE_BRIGHTNESS         255       // 100% (255/255)
#define NVS_DEVICE_CAMERA_ID          1

// ============================================================================
// LED 색상 기본값 (RGB)
// ============================================================================

// PROGRAM (Red)
#define NVS_LED_PROGRAM_R             255
#define NVS_LED_PROGRAM_G             0
#define NVS_LED_PROGRAM_B             0

// PREVIEW (Green)
#define NVS_LED_PREVIEW_R             0
#define NVS_LED_PREVIEW_G             255
#define NVS_LED_PREVIEW_B             0

// OFF (검정 - 변경 가능)
#define NVS_LED_OFF_R                 0
#define NVS_LED_OFF_G                 0
#define NVS_LED_OFF_B                 0

// BATTERY_LOW (Yellow)
#define NVS_LED_BATTERY_LOW_R         255
#define NVS_LED_BATTERY_LOW_G         255
#define NVS_LED_BATTERY_LOW_B         0
