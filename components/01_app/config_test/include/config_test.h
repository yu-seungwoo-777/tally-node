/**
 * @file config_test.h
 * @brief ConfigService 테스트 앱
 */

#ifndef CONFIG_TEST_H
#define CONFIG_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ConfigService 테스트 앱 초기화 및 시작
 */
esp_err_t config_test_app_init(void);

/**
 * @brief ConfigService 테스트 앱 정지
 */
void config_test_app_stop(void);

/**
 * @brief ConfigService 테스트 앱 해제
 */
void config_test_app_deinit(void);

/**
 * @brief 1초마다 호출 - 배터리/업타임 표시
 */
void config_test_app_tick(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_TEST_H
