/**
 * @file web_server_cache.h
 * @brief Web Server 내부 데이터 캐시 모듈
 */

#ifndef TALLY_WEB_SERVER_CACHE_H
#define TALLY_WEB_SERVER_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"

// ============================================================================
// 내부 데이터 캐시 구조체 (event_bus 구조체 그대로 사용)
// ============================================================================

typedef struct {
    system_info_event_t system;       // EVT_INFO_UPDATED
    bool system_valid;

    switcher_status_event_t switcher; // EVT_SWITCHER_STATUS_CHANGED
    bool switcher_valid;

    network_status_event_t network;   // EVT_NETWORK_STATUS_CHANGED
    bool network_valid;

    config_data_event_t config;       // EVT_CONFIG_DATA_CHANGED
    bool config_valid;

    // LoRa 스캔 결과
    lora_scan_complete_t lora_scan;   // EVT_LORA_SCAN_COMPLETE
    bool lora_scan_valid;
    bool lora_scanning;               // 스캔 중 여부
    uint8_t lora_scan_progress;       // 스캔 진행률

    // 디바이스 리스트 (TX 전용)
    device_list_event_t devices;      // EVT_DEVICE_LIST_CHANGED
    bool devices_valid;

    // 라이센스 상태
    license_state_event_t license;    // EVT_LICENSE_STATE_CHANGED
    bool license_valid;
} web_server_data_t;

// ============================================================================
// LED 색상 캐시 구조체
// ============================================================================

typedef struct {
    bool initialized;
    struct { uint8_t r, g, b; } program;
    struct { uint8_t r, g, b; } preview;
    struct { uint8_t r, g, b; } off;
} web_server_led_colors_t;

// ============================================================================
// 캐시 초기화 함수
// ============================================================================

/**
 * @brief 내부 데이터 캐시 초기화
 * @details 캐시 구조체를 0으로 초기화하고 뮤텍스를 생성합니다
 */
void web_server_cache_init(void);

/**
 * @brief 캐시 뮤텍스 획득
 * @return pdTRUE 성공, pdFALSE 실패
 */
 BaseType_t web_server_cache_lock(void);

/**
 * @brief 캐시 뮤텍스 해제
 */
void web_server_cache_unlock(void);

/**
 * @brief 캐시 무효화 (web_server_stop에서 호출)
 */
void web_server_cache_invalidate(void);

/**
 * @brief 캐시 뮤텍스 해제 (web_server_stop에서 호출)
 */
void web_server_cache_deinit(void);

// ============================================================================
// 캐시 데이터 접근 함수
// ============================================================================

/**
 * @brief 시스템 정보 캐시 업데이트
 * @param info 시스템 정보 이벤트 데이터
 */
void web_server_cache_update_system(const system_info_event_t* info);

/**
 * @brief 스위처 상태 캐시 업데이트
 * @param status 스위처 상태 이벤트 데이터
 */
void web_server_cache_update_switcher(const switcher_status_event_t* status);

/**
 * @brief 네트워크 상태 캐시 업데이트
 * @param status 네트워크 상태 이벤트 데이터
 */
void web_server_cache_update_network(const network_status_event_t* status);

/**
 * @brief 설정 데이터 캐시 업데이트
 * @param config 설정 데이터 이벤트 데이터
 */
void web_server_cache_update_config(const config_data_event_t* config);

/**
 * @brief LoRa 스캔 시작 상태 설정
 */
void web_server_cache_set_lora_scan_starting(void);

/**
 * @brief LoRa 스캔 진행률 업데이트
 * @param progress 진행 이벤트 데이터
 */
void web_server_cache_update_lora_scan_progress(const lora_scan_progress_t* progress);

/**
 * @brief LoRa 스캔 완료 결과 업데이트
 * @param result 스캔 완료 이벤트 데이터
 */
void web_server_cache_update_lora_scan_complete(const lora_scan_complete_t* result);

/**
 * @brief 디바이스 리스트 캐시 업데이트
 * @param devices_list 디바이스 리스트 이벤트 데이터
 */
void web_server_cache_update_devices(const device_list_event_t* devices_list);

/**
 * @brief 라이센스 상태 캐시 업데이트
 * @param license 라이센스 상태 이벤트 데이터
 */
void web_server_cache_update_license(const license_state_event_t* license);

/**
 * @brief LoRa 스캔 중지 상태 설정
 */
void web_server_cache_set_lora_scan_stopped(void);

// ============================================================================
// 캐시 데이터 읽기 함수 (뮤텍스 잠금 필요 없음)
// ============================================================================

/**
 * @brief 캐시 데이터 포인터 가져오기 (읽기 전용)
 * @return 캐시 구조체 포인터
 */
const web_server_data_t* web_server_cache_get(void);

/**
 * @brief 시스템 정보 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_system_valid(void);

/**
 * @brief 스위처 상태 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_switcher_valid(void);

/**
 * @brief 네트워크 상태 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_network_valid(void);

/**
 * @brief 설정 데이터 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_config_valid(void);

/**
 * @brief LoRa 스캔 결과 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_lora_scan_valid(void);

/**
 * @brief LoRa 스캔 중 여부 확인
 * @return true 스캔 중, false 스캔 아님
 */
bool web_server_cache_is_lora_scanning(void);

/**
 * @brief LoRa 스캔 진행률 가져오기
 * @return 진행률 (0-100)
 */
uint8_t web_server_cache_get_lora_scan_progress(void);

/**
 * @brief 디바이스 리스트 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_devices_valid(void);

/**
 * @brief 라이센스 상태 유효성 확인
 * @return true 유효함, false 무효함
 */
bool web_server_cache_is_license_valid(void);

// ============================================================================
// LED 색상 캐시 함수
// ============================================================================

/**
 * @brief LED 색상 캐시 초기화 여부 확인
 * @return true 초기화됨, false 초기화 안됨
 */
bool web_server_cache_is_led_colors_initialized(void);

/**
 * @brief LED 색상 캐시 업데이트
 * @param colors LED 색상 이벤트 데이터
 */
void web_server_cache_update_led_colors(const led_colors_event_t* colors);

/**
 * @brief LED 색상 캐시 가져오기
 * @return LED 색상 캐시 구조체 포인터
 */
const web_server_led_colors_t* web_server_cache_get_led_colors(void);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_CACHE_H
