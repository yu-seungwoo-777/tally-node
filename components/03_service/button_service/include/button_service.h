/**
 * @file button_service.h
 * @brief 버튼 서비스
 *
 * 버튼 폴링을 관리하는 서비스 레이어
 */

#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "button_poll.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 버튼 서비스 초기화
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_service_init(void);

/**
 * @brief 버튼 서비스 시작
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_service_start(void);

/**
 * @brief 버튼 서비스 중지
 */
void button_service_stop(void);

/**
 * @brief 버튼 서비스 해제
 */
void button_service_deinit(void);

/**
 * @brief 버튼 콜백 설정
 *
 * @param callback 콜백 함수 (NULL로 설정 시 콜백 비활성화)
 */
void button_service_set_callback(button_callback_t callback);

/**
 * @brief 버튼 현재 상태 확인
 *
 * @return true 버튼 눌림, false 버튼 안 눌림
 */
bool button_service_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_SERVICE_H
