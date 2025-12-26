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

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H
