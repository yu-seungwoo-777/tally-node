/**
 * @file PageManager.h
 * @brief TALLY-NODE Page Manager
 *
 * 여러 디스플레이 페이지를 관리하고 전환
 * - Boot, RX, TX 페이지 관리
 * - 버튼 입력을 통한 페이지 전환
 */

#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 페이지 타입 정의
typedef enum {
    PAGE_TYPE_BOOT = 0,    // 부트 페이지
    PAGE_TYPE_RX,          // RX 페이지
    PAGE_TYPE_TX,          // TX 페이지
    PAGE_TYPE_SETTINGS,    // 설정 페이지
    PAGE_TYPE_NONE         // 페이지 없음
} PageType_t;

/**
 * @brief PageManager 초기화
 *
 * @return esp_err_t ESP_OK 성공, 그 외 에러
 */
esp_err_t PageManager_init(void);

/**
 * @brief 페이지 전환
 *
 * @param page_type 전환할 페이지 타입
 * @return esp_err_t ESP_OK 성공, 그 외 에러
 */
esp_err_t PageManager_switchPage(PageType_t page_type);

/**
 * @brief 현재 페이지 타입 가져오기
 *
 * @return PageType_t 현재 페이지 타입
 */
PageType_t PageManager_getCurrentPage(void);

/**
 * @brief 버튼 입력 처리 (페이지 전환용)
 *
 * @param button_id 버튼 ID (0: 왼쪽/위, 1: 오른쪽/아래)
 */
void PageManager_handleButton(int button_id);

/**
 * @brief 페이지 업데이트 (주기적 호출용)
 */
void PageManager_update(void);

/**
 * @brief 페이지 즉각 업데이트 (즉시 반영 필요시)
 */
void PageManager_updateImmediate(void);

// 페이지별 제어 함수들
/**
 * @brief RX1 상태 설정
 *
 * @param active true: 활성, false: 비활성
 */
void PageManager_setRx1(bool active);

/**
 * @brief RX2 상태 설정
 *
 * @param active true: 활성, false: 비활성
 */
void PageManager_setRx2(bool active);

/**
 * @brief 설정 페이지 진입
 */
void PageManager_enterSettings(void);

/**
 * @brief 설정 페이지 퇴출
 */
void PageManager_exitSettings(void);

/**
 * @brief 롱프레스 시작 처리
 *
 * @param button_id 버튼 ID
 */
void PageManager_handleLongPress(int button_id);

/**
 * @brief 롱프레스 해제 처리
 *
 * @param button_id 버튼 ID
 */
void PageManager_handleLongPressRelease(int button_id);

// SwitcherManager 관련 (TX 전용)
#ifdef DEVICE_MODE_TX

// Switcher 인덱스 정의
#define PAGE_SWITCHER_PRIMARY   0
#define PAGE_SWITCHER_SECONDARY 1

/**
 * @brief 스위처 연결 상태 가져오기
 *
 * @param index 스위처 인덱스 (PAGE_SWITCHER_PRIMARY, PAGE_SWITCHER_SECONDARY)
 * @return true 연결됨, false 연결 안됨
 */
bool PageManager_isSwitcherConnected(int index);

/**
 * @brief 듀얼 모드 상태 가져오기
 *
 * @return true 듀얼 모드, false 싱글 모드
 */
bool PageManager_getDualMode(void);

/**
 * @brief 스위처 타입 문자열 가져오기
 *
 * @param index 스위처 인덱스
 * @return 타입 문자열 ("ATEM", "VMIX", "OBS", "OSEE", "NONE")
 */
const char* PageManager_getSwitcherType(int index);

/**
 * @brief 스위처 IP 주소 가져오기
 *
 * @param index 스위처 인덱스
 * @return IP 주소 문자열
 */
const char* PageManager_getSwitcherIp(int index);

/**
 * @brief 스위처 포트 가져오기
 *
 * @param index 스위처 인덱스
 * @return 포트 번호
 */
uint16_t PageManager_getSwitcherPort(int index);


#endif

#ifdef __cplusplus
}
#endif

#endif // PAGE_MANAGER_H