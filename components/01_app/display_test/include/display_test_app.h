/**
 * @file display_test_app.h
 * @brief 디스플레이 테스트 앱
 *
 * 01_app 계층 - 애플리케이션
 * - DisplayManager, BootPage 사용하여 부팅 화면 테스트
 */

#ifndef DISPLAY_TEST_APP_H
#define DISPLAY_TEST_APP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 디스플레이 테스트 앱 초기화
 *
 * @return ESP_OK 성공
 */
esp_err_t display_test_app_init(void);

/**
 * @brief 디스플레이 테스트 앱 실행
 *
 * @return ESP_OK 성공
 */
esp_err_t display_test_app_start(void);

/**
 * @brief 디스플레이 테스트 앱 정지
 */
void display_test_app_stop(void);

/**
 * @brief 실행 중 여부 확인
 *
 * @return true 실행 중
 */
bool display_test_app_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_TEST_APP_H
