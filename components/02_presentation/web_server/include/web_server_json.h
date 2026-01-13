/**
 * @file web_server_json.h
 * @brief Web Server JSON 생성 함수 모듈
 */

#ifndef TALLY_WEB_SERVER_JSON_H
#define TALLY_WEB_SERVER_JSON_H

#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 네트워크 JSON 생성 함수
// ============================================================================

/**
 * @brief 네트워크 AP 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_network_ap(void);

/**
 * @brief 네트워크 WiFi 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_network_wifi(void);

/**
 * @brief 네트워크 Ethernet 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_network_ethernet(void);

// ============================================================================
// Tally JSON 생성 함수
// ============================================================================

/**
 * @brief Tally 데이터 JSON 생성 (PGM/PVW 리스트 + Raw hex)
 * @param tally_data Tally 데이터 배열
 * @param channel_count 채널 수
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_tally(const uint8_t* tally_data, uint8_t channel_count);

/**
 * @brief 빈 Tally 데이터 JSON 생성 (연결 안 됨)
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_empty_tally(void);

// ============================================================================
// 스위처 JSON 생성 함수
// ============================================================================

/**
 * @brief 스위처 Primary 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_switcher_primary(void);

/**
 * @brief 스위처 Secondary 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_switcher_secondary(void);

/**
 * @brief 스위처 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_switcher(void);

// ============================================================================
// 시스템 JSON 생성 함수
// ============================================================================

/**
 * @brief 시스템 상태 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_system(void);

// ============================================================================
// RF/Broadcast JSON 생성 함수
// ============================================================================

/**
 * @brief RF 설정 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_rf(void);

/**
 * @brief 브로드캐스트 설정 JSON 생성
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_broadcast(void);

// ============================================================================
// 라이센스 JSON 생성 함수
// ============================================================================

/**
 * @brief 라이센스 상태 JSON 생성 (이벤트 캐시 사용)
 * @return cJSON 객체 포인터 (사용 후 cJSON_Delete로 해제)
 */
cJSON* web_server_json_create_license(void);

// ============================================================================
// 헬퍼 함수
// ============================================================================

/**
 * @brief packed 데이터에서 채널 상태 가져오기
 * @param data packed 데이터
 * @param channel 채널 번호 (1-20)
 * @return 0=off, 1=pgm, 2=pvw, 3=both
 */
uint8_t web_server_json_get_channel_state(const uint8_t* data, uint8_t channel);

/**
 * @brief packed 데이터를 hex 문자열로 변환
 * @param data 변환할 바이트 배열
 * @param size 데이터 크기
 * @param out 출력 버퍼
 * @param out_size 출력 버퍼 크기 (최소 size * 2 + 1)
 * @details 바이트 배열을 대문자 16진수 문자열로 변환합니다 (예: {0xAB, 0xCD} → "ABCD")
 */
void web_server_json_packed_to_hex(const uint8_t* data, uint8_t size, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_JSON_H
