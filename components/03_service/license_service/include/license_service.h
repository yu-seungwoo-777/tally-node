/**
 * @file license_service.h
 * @brief 라이센스 상태 관리 서비스
 *
 * - 라이센스 검증 (LicenseClient 통해)
 * - NVS에 device_limit 저장
 * - 30분 유예 기간 관리
 * - 라이센스 상태 추적
 */

#ifndef LICENSE_SERVICE_H
#define LICENSE_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 상수 정의
// ============================================================================

#define LICENSE_KEY_LEN        16      // 라이센스 키 길이
#define LICENSE_GRACE_PERIOD_SEC (30 * 60)  // 30분 유예 시간

// ============================================================================
// 라이센스 상태
// ============================================================================

typedef enum {
    LICENSE_STATE_VALID = 0,       // 인증됨 (device_limit > 0)
    LICENSE_STATE_INVALID,         // 인증 실패 (device_limit == 0)
    LICENSE_STATE_GRACE,           // 유예 기간 (30분)
    LICENSE_STATE_CHECKING,        // 검증 중
} license_state_t;

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
 *
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
 * @return true 유효함 (VALID 또는 GRACE 상태)
 */
bool license_service_is_valid(void);

/**
 * @brief 현재 상태 가져오기
 * @return 라이센스 상태
 */
license_state_t license_service_get_state(void);

/**
 * @brief 유예 기간 활성화 확인
 * @return true 유예 기간 중
 */
bool license_service_is_grace_active(void);

/**
 * @brief 유예 기간 남은 시간 (초)
 * @return 남은 시간 (초), 0=유예 기간 아님
 */
uint32_t license_service_get_grace_remaining(void);

/**
 * @brief Tally 전송 가능 여부 확인
 * @return true 전송 가능 (VALID 또는 GRACE 상태)
 */
bool license_service_can_send_tally(void);

/**
 * @brief 라이센스 키 가져오기
 * @param out_key[out] 라이센스 키 버퍼 (17바이트 이상)
 * @return ESP_OK 성공
 */
esp_err_t license_service_get_key(char* out_key);

#ifdef __cplusplus
}
#endif

#endif // LICENSE_SERVICE_H
