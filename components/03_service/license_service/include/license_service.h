/**
 * @file license_service.h
 * @brief 라이센스 상태 관리 서비스
 *
 * - 라이센스 검증 (LicenseClient 통해)
 * - NVS에 device_limit 저장
 * - 라이센스 상태 추적
 */

#ifndef LICENSE_SERVICE_H
#define LICENSE_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 상수 정의
// ============================================================================

#define LICENSE_KEY_LEN        16      // 라이센스 키 길이

// license_state_t은 event_bus.h에 정의됨

// ============================================================================
// API
// ============================================================================

/**
 * @brief License Service 초기화
 * @return ESP_OK 성공
 */
esp_err_t license_service_init(void);

/**
 * @brief License Service 시작 (이벤트 구독)
 * @return ESP_OK 성공
 */
esp_err_t license_service_start(void);

/**
 * @brief License Service 정지 (이벤트 구독 해제)
 */
void license_service_stop(void);

/**
 * @brief 라이센스 검증 요청
 * @param key 라이센스 키 (16자리)
 * @return ESP_OK 성공
 */
esp_err_t license_service_validate(const char* key);

/**
 * @brief device_limit 가져오기
 * @return device_limit (0=미등록)
 */
uint8_t license_service_get_device_limit(void);

/**
 * @brief 라이센스 유효성 확인
 * @return true 유효함
 */
bool license_service_is_valid(void);

/**
 * @brief 현재 상태 가져오기
 * @return 라이센스 상태
 */
license_state_t license_service_get_state(void);

/**
 * @brief Tally 전송 가능 여부 확인
 * @return true 전송 가능
 */
bool license_service_can_send_tally(void);

/**
 * @brief 라이센스 키 가져오기
 * @param out_key 라이센스 키 버퍼 (17바이트 이상)
 * @return ESP_OK 성공
 */
esp_err_t license_service_get_key(char* out_key);

/**
 * @brief 라이센스 서버 연결 테스트
 * @return true 연결 성공, false 연결 실패
 */
bool license_service_connection_test(void);

#ifdef __cplusplus
}
#endif

#endif // LICENSE_SERVICE_H
