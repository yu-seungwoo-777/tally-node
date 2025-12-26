/**
 * @file rx_command.h
 * @brief RX 명령 수신 및 실행 서비스
 *
 * TX에서 오는 관리 명령을 수신하고 실행
 */

#ifndef RX_COMMAND_H
#define RX_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// RX 상태 콜백 (TX로 전송할 상태 정보)
// @note RSSI/SNR은 TX에서 수신 시 직접 취득하므로 포함하지 않음
typedef struct {
    uint8_t battery;       // 배터리 0-100%
    uint8_t camera_id;     // 카메라 ID
    uint32_t uptime;       // 업타임 (초)
    uint8_t brightness;    // 밝기 0-100
    bool is_stopped;       // 기능 정지 상태
} rx_status_t;

typedef void (*rx_command_get_status_callback_t)(rx_status_t* status);

/**
 * @brief RX 명령 서비스 초기화
 * @param get_status_cb 상태 요청 시 호출할 콜백 (배터리, 카메라 ID 등 제공)
 */
esp_err_t rx_command_init(rx_command_get_status_callback_t get_status_cb);

/**
 * @brief RX 명령 서비스 시작
 */
esp_err_t rx_command_start(void);

/**
 * @brief RX 명령 서비스 정지
 */
void rx_command_stop(void);

/**
 * @brief LoRa 패킷 수신 처리 (lora_service 콜백에서 호출)
 * @param data 수신 데이터
 * @param length 데이터 길이
 */
void rx_command_process_packet(const uint8_t* data, size_t length);

/**
 * @brief Device ID 설정 (MAC 주소 뒤 4자리)
 */
void rx_command_set_device_id(const uint8_t* device_id);

/**
 * @brief Device ID 가져오기
 */
const uint8_t* rx_command_get_device_id(void);

#ifdef __cplusplus
}
#endif

#endif // RX_COMMAND_H
