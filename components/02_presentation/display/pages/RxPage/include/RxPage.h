/**
 * @file RxPage.h
 * @brief RX 모드 페이지 (LoRa 수신 Tally 상태)
 *
 * 3개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 시스템 정보 (3x2 테이블)
 * - Page 3: RX 수신 통계 (RSSI, SNR, Tally 수신 간격)
 *
 * 페이지 상태:
 * - PAGE_STATE_NORMAL: 일반 페이지 (Tally/System/RX Stats 전환)
 * - PAGE_STATE_CAMERA_ID: 카메라 ID 변경 팝업
 */

#ifndef RX_PAGE_H
#define RX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "DisplayManager.h"

// ========== 페이지 상수 ==========

/**
 * @brief RX 모드 페이지 수
 */
#define RX_PAGE_COUNT 3

// Tally 타입 전방 선언 (나중에 tally_types.h로 교체)
typedef enum {
    TALLY_STATE_OFF = 0,
    TALLY_STATE_SAFE,   // 안전 상태
    TALLY_STATE_PVW,
    TALLY_STATE_PGM
} tally_state_t;

// RxPage 상태
typedef enum {
    RX_PAGE_STATE_NORMAL = 0,     // 일반 페이지 상태
    RX_PAGE_STATE_CAMERA_ID       // 카메라 ID 변경 팝업 상태
} rx_page_state_t;

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

// ========== RX 통계 데이터 설정 ==========

/**
 * @brief RX 수신 통계 설정
 * @param rssi 마지막 RSSI (dBm)
 * @param snr 마지막 SNR (dB)
 * @param interval 마지막 Tally 패킷 수신 간격 (ms)
 */
void rx_page_set_rx_stats(int16_t rssi, float snr, uint32_t interval);

// ========== 페이지 제어 ==========

/**
 * @brief 페이지 전환
 * @param page 페이지 번호 (1: Tally, 2: System, 3: RX Stats)
 */
void rx_page_switch_page(uint8_t page);

/**
 * @brief 현재 페이지 가져오기
 * @return 페이지 번호 (1: Tally, 2: System, 3: RX Stats)
 */
uint8_t rx_page_get_current_page(void);

/**
 * @brief 전체 페이지 수 가져오기
 * @return 페이지 수 (RX_PAGE_COUNT)
 */
uint8_t rx_page_get_page_count(void);

// ========== 카메라 ID 변경 팝업 제어 ==========

/**
 * @brief 페이지 상태 설정
 * @param state 페이지 상태 (RX_PAGE_STATE_NORMAL, RX_PAGE_STATE_CAMERA_ID)
 */
void rx_page_set_state(rx_page_state_t state);

/**
 * @brief 페이지 상태 가져오기
 * @return 현재 페이지 상태
 */
rx_page_state_t rx_page_get_state(void);

/**
 * @brief 카메라 ID 변경 팝업 표시 시작
 */
void rx_page_show_camera_id_popup(void);

/**
 * @brief 카메라 ID 변경 팝업 표시 시작 (최대값 지정)
 * @param max_camera_num 최대 카메라 번호
 */
void rx_page_show_camera_id_popup_with_max(uint8_t max_camera_num);

/**
 * @brief 카메라 ID 변경 팝업 숨김 (일반 페이지로 복귀)
 */
void rx_page_hide_camera_id_popup(void);

/**
 * @brief 현재 표시 중인 카메라 ID 가져오기
 * @return 현재 카메라 ID
 */
uint8_t rx_page_get_display_camera_id(void);

/**
 * @brief 표시용 카메라 ID 직접 설정 (롱프레스 중 자동 변경용)
 * @param cam_id 카메라 ID (1-max_camera_num)
 */
void rx_page_set_display_camera_id(uint8_t cam_id);

/**
 * @brief 카메라 ID 변경 중 여부 설정
 * @param changing true: 변경 중, false: 정지
 */
void rx_page_set_camera_id_changing(bool changing);

/**
 * @brief 카메라 ID 변경 중인지 확인
 * @return true: 변경 중, false: 정지됨
 */
bool rx_page_is_camera_id_changing(void);

/**
 * @brief 카메라 ID 변경 타이머 콜백 (0.8초마다 호출)
 * @param max_camera_num 최대 카메라 번호
 * @return 다음 카메라 ID
 */
uint8_t rx_page_cycle_camera_id(uint8_t max_camera_num);

// ========== 기능 정지 상태 제어 ==========

/**
 * @brief 기능 정지 상태 설정
 * @param stopped true: 정지 상태, false: 정상 상태
 */
void rx_page_set_stopped(bool stopped);

/**
 * @brief 기능 정지 상태 확인
 * @return true: 정지 상태, false: 정상 상태
 */
bool rx_page_is_stopped(void);

#ifdef __cplusplus
}
#endif

#endif // RX_PAGE_H
