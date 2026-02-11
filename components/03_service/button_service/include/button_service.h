/**
 * @file ButtonService.h
 * @brief 버튼 서비스 - 단일 버튼 폴링 및 이벤트 발행
 *
 * - EoRa-S3 내장 버튼 (GPIO 0) 폴링
 * - Active Low (누르면 0)
 * - 10ms 폴링 주기
 * - 20ms 디바운싱
 * - 기본 1000ms 롱 프레스 (TX: 5000ms, RX: 1000ms)
 * - event_bus로 이벤트 발행
 */

#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 버튼 서비스 초기화
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t button_service_init(void);

/**
 * @brief 버튼 서비스 시작
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
 * @brief 버튼 현재 상태 확인
 * @return true 버튼 눌림, false 버튼 안 눌림
 */
bool button_service_is_pressed(void);

/**
 * @brief 버튼 서비스 초기화 여부
 * @return true 초기화됨, false 초기화되지 않음
 */
bool button_service_is_initialized(void);

/**
 * @brief 롱프레스 시간 설정
 * @param ms 롱프레스 시간 (밀리초)
 */
void button_service_set_long_press_time(uint32_t ms);

/**
 * @brief 롱프레스 시간 반환
 * @return 롱프레스 시간 (ms)
 */
uint32_t button_service_get_long_press_time(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_SERVICE_H
