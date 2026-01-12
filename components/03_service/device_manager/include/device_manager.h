/**
 * @file device_manager.h
 * @brief Device Manager - TX/RX 디바이스 관리
 *
 * TX: 주기적 상태 요청, RX 디바이스 리스트 관리
 * RX: 상태 요청 수신 시 응답 송신
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device Manager 초기화
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t device_manager_init(void);

/**
 * @brief Device Manager 시작
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t device_manager_start(void);

/**
 * @brief Device Manager 정지
 * @return 없음
 */
void device_manager_stop(void);

/**
 * @brief Device Manager 해제
 * @return 없음
 */
void device_manager_deinit(void);

#ifdef DEVICE_MODE_TX

/**
 * @brief 상태 요청 주기 설정 (TX 전용)
 * @param interval_ms 요청 주기 (ms)
 * @return 없음
 */
void device_manager_set_request_interval(uint32_t interval_ms);

/**
 * @brief 상태 요청 즉시 송신 (TX 전용)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t device_manager_request_status_now(void);

/**
 * @brief PING 송신 (TX 전용)
 * @param device_id 2바이트 디바이스 ID (NULL이면 Broadcast)
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t device_manager_send_ping(const uint8_t* device_id);

#endif // DEVICE_MODE_TX

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MANAGER_H
