/**
 * @file tx_command.h
 * @brief TX 명령 송신 서비스
 *
 * RX 디바이스로 관리 명령 전송
 */

#ifndef TX_COMMAND_H
#define TX_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TX 명령 서비스 초기화
 */
esp_err_t tx_command_init(void);

/**
 * @brief TX 명령 서비스 시작
 */
esp_err_t tx_command_start(void);

/**
 * @brief TX 명령 서비스 정지
 */
void tx_command_stop(void);

// ========== 명령 전송 함수 ==========

/**
 * @brief 상태 요청 (Broadcast)
 * 모든 RX 디바이스가 응답
 */
esp_err_t tx_command_send_status_req(void);

/**
 * @brief 밝기 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param brightness 밝기 0-100
 */
esp_err_t tx_command_set_brightness(const uint8_t* device_id, uint8_t brightness);

/**
 * @brief 카메라 ID 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param camera_id 카메라 ID
 */
esp_err_t tx_command_set_camera_id(const uint8_t* device_id, uint8_t camera_id);

/**
 * @brief 주파수+SyncWord 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param frequency 주파수 (MHz)
 * @param sync_word sync word
 */
esp_err_t tx_command_set_rf(const uint8_t* device_id, float frequency, uint8_t sync_word);

/**
 * @brief 기능 정지 명령 전송 (Uni/Broadcast)
 * @param device_id 타겟 RX device_id (4바이트), nullptr이면 broadcast
 */
esp_err_t tx_command_send_stop(const uint8_t* device_id);

/**
 * @brief 재부팅 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 */
esp_err_t tx_command_reboot(const uint8_t* device_id);

/**
 * @brief 지연시간 테스트 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param timestamp 송신 시간 (ms)
 */
esp_err_t tx_command_ping(const uint8_t* device_id, uint32_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // TX_COMMAND_H
