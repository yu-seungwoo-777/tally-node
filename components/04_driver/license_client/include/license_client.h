/**
 * @file license_client.h
 * @brief 라이센스 서버 HTTP 클라이언트
 *
 * 라이센스 서버와 HTTPS 통신하여 device_limit을 받아옵니다.
 * - license_key 검증
 * - 연결 테스트
 */

#ifndef LICENSE_CLIENT_H
#define LICENSE_CLIENT_H

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
#define LICENSE_API_KEY         "QNbzOIgjVwtUx36mnG1mStrrTOsFGNW7"
#define LICENSE_SERVER_BASE     "http://tally-node.duckdns.org"
#define LICENSE_VALIDATE_PATH   "/api/validate-license"
#define LICENSE_TIMEOUT_MS      15000   // 15초 타임아웃

// ============================================================================
// 구조체 정의
// ============================================================================

/**
 * @brief 라이센스 검증 응답
 */
typedef struct {
    bool success;               // 성공 여부
    uint8_t device_limit;       // 디바이스 제한 (0=미등록)
    char error[128];            // 에러 메시지
} license_validate_response_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief License Client 초기화
 */
esp_err_t license_client_init(void);

/**
 * @brief 라이센스 검증 요청
 *
 * @param key 라이센스 키 (16자리)
 * @param mac_address MAC 주소 문자열 (예: "AC:67:B2:EA:4B:12")
 * @param connected 네트워크 연결 상태 (license_service에서 확인 후 전달)
 * @param out_response[out] 검증 응답
 * @return esp_err_t ESP_OK 성공
 */
esp_err_t license_client_validate(const char* key, const char* mac_address,
                                  bool connected, license_validate_response_t* out_response);

/**
 * @brief 서버 연결 테스트
 *
 * @return true 연결 성공, false 연결 실패
 */
bool license_client_connection_test(void);

#ifdef __cplusplus
}
#endif

#endif // LICENSE_CLIENT_H
