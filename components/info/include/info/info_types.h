/**
 * @file info_types.h
 * @brief InfoManager 공용 타입 정의
 *
 * C/C++ 공통으로 사용하는 타입과 상수 정의
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 장치 ID 최대 길이
#define INFO_DEVICE_ID_MAX_LEN  16
#define INFO_MAC_ADDR_STR_LEN   18

// NVS 네임스페이스
#define INFO_NVS_NAMESPACE      "info_mgr"

// NVS 키 이름
#define INFO_NVS_KEY_DEVICE_ID  "device_id"
#define INFO_NVS_KEY_ID_TYPE    "id_gen_type"
#define INFO_NVS_KEY_FIRST_BOOT "first_boot"

// ID 생성 타입
typedef enum {
    INFO_ID_GEN_MANUAL = 0,   // 수동 설정
    INFO_ID_GEN_MAC_BASED = 1 // MAC 기반 자동 생성
} info_id_gen_type_t;

// 시스템 정보 구조체
typedef struct {
    char device_id[INFO_DEVICE_ID_MAX_LEN];  // 장치 ID
    char wifi_mac[INFO_MAC_ADDR_STR_LEN];    // WiFi MAC 주소
    float battery_percent;                    // 배터리 잔량 (%)
    float temperature;                        // 온도 (°C)
    uint32_t uptime_sec;                      // 가동 시간 (초)
    uint32_t free_heap;                       // 사용 가능한 힙 메모리
    uint32_t min_free_heap;                   // 최소 힙 메모리
    uint32_t lora_rssi;                       // LoRa RSSI (단위: 0.1dBm)
    uint32_t lora_snr;                        // LoRa SNR (단위: 0.1dB)
    uint32_t packet_count_tx;                 // 송신 패킷 수
    uint32_t packet_count_rx;                 // 수신 패킷 수
    uint32_t error_count;                     // 에러 횟수
} info_system_info_t;

// Observer 콜백 타입 (함수 포인터 + context)
typedef void (*info_observer_fn_t)(const info_system_info_t* info, void* ctx);

// Observer 핸들 (불투명 포인터)
typedef struct info_observer* info_observer_handle_t;

// 시스템 정보 초기화
void info_system_info_init(info_system_info_t* info);

#ifdef __cplusplus
}
#endif