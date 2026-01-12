/**
 * @file tally_test_service.h
 * @brief Tally 테스트 모드 서비스
 *
 * PGM/PVW 패턴을 순환하며 Tally 데이터를 전송하는 테스트 모드
 */

#ifndef TALLY_TEST_SERVICE_H
#define TALLY_TEST_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 테스트 모드 초기화
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t tally_test_service_init(void);

/**
 * @brief 테스트 모드 시작
 * @param max_channels 최대 채널 수 (1-20)
 * @param interval_ms 송신 간격 (100-3000ms)
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 파라미터 오류
 */
esp_err_t tally_test_service_start(uint8_t max_channels, uint16_t interval_ms);

/**
 * @brief 테스트 모드 중지
 * @return 없음
 */
void tally_test_service_stop(void);

/**
 * @brief 테스트 모드 실행 중 여부 확인
 * @return true 실행 중, false 정지됨
 */
bool tally_test_service_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // TALLY_TEST_SERVICE_H
