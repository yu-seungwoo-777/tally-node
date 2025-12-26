/**
 * @file RxPage.h
 * @brief RX 모드 페이지 (LoRa 수신 Tally 상태)
 *
 * 2개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 시스템 정보 (3x2 테이블)
 */

#ifndef RX_PAGE_H
#define RX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "DisplayManager.h"

// Tally 타입 전방 선언 (나중에 tally_types.h로 교체)
typedef enum {
    TALLY_STATE_OFF = 0,
    TALLY_STATE_SAFE,   // 안전 상태
    TALLY_STATE_PVW,
    TALLY_STATE_PGM
} tally_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RxPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool rx_page_init(void);

// ========== Tally 데이터 설정 ==========

/**
 * @brief PGM 채널 목록 설정
 * @param channels 채널 배열
 * @param count 채널 수 (최대 20개)
 */
void rx_page_set_pgm_channels(const uint8_t* channels, uint8_t count);

/**
 * @brief PVW 채널 목록 설정
 * @param channels 채널 배열
 * @param count 채널 수 (최대 20개)
 */
void rx_page_set_pvw_channels(const uint8_t* channels, uint8_t count);

/**
 * @brief CAM ID 설정
 * @param cam_id 카메라 ID (1-255)
 */
void rx_page_set_cam_id(uint8_t cam_id);

// ========== 시스템 정보 설정 ==========

/**
 * @brief 배터리 퍼센트 설정
 * @param percent 배터리 퍼센트 (0-100)
 */
void rx_page_set_battery(uint8_t percent);

/**
 * @brief LoRa RSSI 설정
 * @param rssi RSSI 값 (dBm)
 */
void rx_page_set_rssi(int16_t rssi);

/**
 * @brief LoRa SNR 설정
 * @param snr SNR 값 (dB)
 */
void rx_page_set_snr(float snr);

/**
 * @brief 주파수 설정
 * @param freq_mhz 주파수 (MHz)
 */
void rx_page_set_frequency(float freq_mhz);

/**
 * @brief 동기 워드 설정
 * @param sync_word 동기 워드
 */
void rx_page_set_sync_word(uint8_t sync_word);

/**
 * @brief 전압 설정
 * @param voltage 전압 (V)
 */
void rx_page_set_voltage(float voltage);

/**
 * @brief 온도 설정
 * @param temp 온도 (°C)
 */
void rx_page_set_temperature(float temp);

/**
 * @brief Device ID 설정
 * @param device_id Device ID (최대 16자)
 */
void rx_page_set_device_id(const char* device_id);

/**
 * @brief Uptime 설정
 * @param uptime_sec 가동 시간 (초)
 */
void rx_page_set_uptime(uint64_t uptime_sec);

// ========== 페이지 제어 ==========

/**
 * @brief 페이지 전환
 * @param page 페이지 번호 (1: Tally, 2: System)
 */
void rx_page_switch_page(uint8_t page);

/**
 * @brief 현재 페이지 가져오기
 * @return 페이지 번호 (1: Tally, 2: System)
 */
uint8_t rx_page_get_current_page(void);

#ifdef __cplusplus
}
#endif

#endif // RX_PAGE_H
