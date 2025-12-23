/**
 * @file lora_test_app.h
 * @brief LoRa 테스트 앱
 *
 * 01_app 계층 - 애플리케이션
 * - LoRaService 사용하여 테스트 송신
 * - event_bus로 이벤트 구독/발행
 */

#ifndef LORA_TEST_APP_H
#define LORA_TEST_APP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LoRa 테스트 앱 초기화 및 시작
 *
 * @return ESP_OK 성공
 */
esp_err_t lora_test_app_init(void);

/**
 * @brief LoRa 테스트 앱 실행
 *
 * 내부 태스크를 생성하여 테스트 송신 반복
 *
 * @return ESP_OK 성공
 */
esp_err_t lora_test_app_start(void);

/**
 * @brief LoRa 테스트 앱 정지
 */
void lora_test_app_stop(void);

/**
 * @brief LoRa 테스트 앱 해제
 */
void lora_test_app_deinit(void);

/**
 * @brief 실행 중 여부 확인
 *
 * @return true 실행 중
 */
bool lora_test_app_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // LORA_TEST_APP_H
