/**
 * @file DisplayManager.h
 * @brief 디스플레이 관리자 (C++)
 *
 * OLED 디스플레이 페이지 전환 및 렌더링 관리
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 타입 정의
// ============================================================================

/**
 * @brief 페이지 ID
 */
typedef enum {
    PAGE_NONE = 0,    // 초기 상태
    PAGE_BOOT,        // 부팅 화면 (공용)
    PAGE_TX,          // TX 모드 페이지
    PAGE_RX,          // RX 모드 페이지
    PAGE_BATTERY_EMPTY, // 배터리 비움 화면 (TX/RX 공용)
    PAGE_COUNT        // 페이지 수
} display_page_t;

/**
 * @brief 페이지 인터페이스
 *
 * 각 페이지는 이 인터페이스를 구현해야 함
 */
typedef struct {
    display_page_t id;               // 페이지 ID
    const char* name;                // 페이지 이름
    void (*init)(void);              // 초기화
    void (*render)(u8g2_t* u8g2);    // 렌더링
    void (*on_enter)(void);          // 페이지 진입 시 호출
    void (*on_exit)(void);           // 페이지 퇴장 시 호출
    void (*timer_tick)(void);        // 1초 간격 타이머 틱 (NULL 가능)
} display_page_interface_t;

// ============================================================================
// 공개 API
// ============================================================================

/**
 * @brief DisplayManager 초기화
 * @return true 성공, false 실패
 */
bool display_manager_init(void);

/**
 * @brief DisplayManager 시작
 */
void display_manager_start(void);

/**
 * @brief DisplayManager 정지
 */
void display_manager_stop(void);

/**
 * @brief 디스플레이 새로고침 주기 설정
 * @param interval_ms 갱신 주기 (ms)
 */
void display_manager_set_refresh_interval(uint32_t interval_ms);

/**
 * @brief 페이지 등록
 * @param page_interface 페이지 인터페이스
 * @return true 성공, false 실패
 */
bool display_manager_register_page(const display_page_interface_t* page_interface);

/**
 * @brief 페이지 전환
 * @param page_id 전환할 페이지 ID
 */
void display_manager_set_page(display_page_t page_id);

/**
 * @brief 현재 페이지 ID 가져오기
 * @return 현재 페이지 ID
 */
display_page_t display_manager_get_current_page(void);

/**
 * @brief 디스플레이 강제 갱신
 */
void display_manager_force_refresh(void);

/**
 * @brief 디스플레이 전원 켜기/끄기
 * @param on true = 켜기, false = 끄기
 */
void display_manager_set_power(bool on);

/**
 * @brief U8g2 인스턴스 가져오기 (페이지 렌더링용)
 * @return U8g2 포인터
 */
u8g2_t* display_manager_get_u8g2(void);

// ============================================================================
// BootPage 편의 API (DisplayManager를 통해 BootPage 제어)
// ============================================================================

/**
 * @brief 부팅 메시지 설정
 * @param message 표시할 메시지
 */
void display_manager_boot_set_message(const char* message);

/**
 * @brief 진행률 설정
 * @param progress 진행률 (0-100)
 */
void display_manager_boot_set_progress(uint8_t progress);

/**
 * @brief 부팅 완료 후 기본 페이지로 전환
 *
 * 빌드 환경(DEVICE_MODE_TX/DEVICE_MODE_RX)에 따라
 * TX 또는 RX 페이지로 자동 전환합니다.
 */
void display_manager_boot_complete(void);

// ============================================================================
// 페이지 편의 API (DisplayManager를 통해 TxPage/RxPage 제어)
// ============================================================================

/**
 * @brief 현재 서브 페이지 인덱스 가져오기
 * @return 현재 서브 페이지 번호 (TX: 1-5, RX: 1-2)
 */
uint8_t display_manager_get_page_index(void);

/**
 * @brief 서브 페이지 전환
 * @param index 서브 페이지 번호 (TX: 1-5, RX: 1-2)
 */
void display_manager_switch_page(uint8_t index);

// ============================================================================
// RxPage 전용 API (DEVICE_MODE_RX일 때만 사용)
// ============================================================================

#ifdef DEVICE_MODE_RX

/**
 * @brief RxPage 카메라 ID 설정
 * @param cam_id 카메라 ID (1-20)
 */
void display_manager_set_cam_id(uint8_t cam_id);

/**
 * @brief RxPage 상태 가져오기 (0=Normal, 1=CameraID)
 * @return 현재 페이지 상태
 */
int display_manager_get_state(void);

/**
 * @brief RxPage 카메라 ID 팝업 표시
 * @param max_camera_num 최대 카메라 번호
 */
void display_manager_show_camera_id_popup(uint8_t max_camera_num);

/**
 * @brief RxPage 카메라 ID 팝업 숨기기
 */
void display_manager_hide_camera_id_popup(void);

/**
 * @brief RxPage 카메라 ID 변경 중 상태 설정
 * @param changing true = 변경 중, false = 정지
 */
void display_manager_set_camera_id_changing(bool changing);

/**
 * @brief RxPage 카메라 ID 변경 중인지 확인
 * @return true = 변경 중, false = 정지
 */
bool display_manager_is_camera_id_changing(void);

/**
 * @brief RxPage 표시중인 카메라 ID 가져오기
 * @return 현재 표시중인 카메라 ID
 */
uint8_t display_manager_get_display_camera_id(void);

/**
 * @brief RxPage 카메라 ID 순환
 * @param max_camera_num 최대 카메라 번호
 * @return 새로운 카메라 ID
 */
uint8_t display_manager_cycle_camera_id(uint8_t max_camera_num);

#endif // DEVICE_MODE_RX

// ============================================================================
// System 데이터 업데이트 API
// ============================================================================

/**
 * @brief System 데이터 업데이트
 * @param device_id 디바이스 ID (4자리 hex 문자열)
 * @param battery 배터리 % (0-100)
 * @param voltage 전압 (V)
 * @param temperature 온도 (°C)
 */
void display_manager_update_system(const char* device_id, uint8_t battery,
                                   float voltage, float temperature);

/**
 * @brief RSSI/SNR 업데이트 (RX 페이지 안테나 아이콘용)
 *
 * @param rssi RSSI (dBm)
 * @param snr SNR (dB)
 */
void display_manager_update_rssi(int16_t rssi, float snr);

/**
 * @brief PGM/PVW 채널 업데이트 (RX 페이지 Tally 표시용)
 *
 * @param pgm_channels PGM 채널 배열
 * @param pgm_count PGM 채널 수
 * @param pvw_channels PVW 채널 배열
 * @param pvw_count PVW 채널 수
 */
void display_manager_update_tally(const uint8_t* pgm_channels, uint8_t pgm_count,
                                  const uint8_t* pvw_channels, uint8_t pvw_count);

/**
 * @brief Ethernet DHCP 모드 업데이트 (TX 페이지)
 *
 * @param dhcp_mode true=DHCP, false=Static
 */
void display_manager_update_ethernet_dhcp_mode(bool dhcp_mode);

// ============================================================================
// 배터리 비움 페이지 API (TX/RX 공통)
// ============================================================================

/**
 * @brief 배터리 비움 상태 설정
 * @param empty true: 배터리 비움 상태, false: 정상 상태
 *
 * 배터리 잔량이 0%일 때 자동으로 배터리 비움 페이지로 전환합니다.
 */
void display_manager_set_battery_empty(bool empty);

/**
 * @brief 배터리 비움 상태 확인
 * @return true: 배터리 비움 상태, false: 정상 상태
 */
bool display_manager_is_battery_empty(void);

/**
 * @brief 딥슬립 카운트다운 설정 (초)
 * @param seconds 남은 시간 (초 단위, 0 = 카운트다운 없음)
 *
 * 배터리 Empty 페이지에서 카운트다운을 표시합니다.
 */
void display_manager_set_deep_sleep_countdown(uint8_t seconds);

/**
 * @brief 딥슬립 카운트다운 확인
 * @return 남은 시간 (초 단위, 0 = 카운트다운 없음)
 */
uint8_t display_manager_get_deep_sleep_countdown(void);

/**
 * @brief 현재 배터리 전압 확인
 * @return 전압 (V)
 */
float display_manager_get_voltage(void);

// ============================================================================
// 디스플레이 갱신
// ============================================================================

/**
 * @brief 디스플레이 갱신 루프 (주기적으로 호출해야 함)
 *
 * @note 이 함수는 메인 루프나 타이머에서 주기적으로 호출되어야 함
 */
void display_manager_update(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
