/**
 * @file SettingsPage.h
 * @brief TALLY-NODE Settings Page
 *
 * 설정 페이지 UI 렌더링 및 사용자 입력 처리
 */

#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SettingsPage 초기화
 *
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t SettingsPage_init(void);

/**
 * @brief 설정 페이지 표시
 */
void SettingsPage_showPage(void);

/**
 * @brief 설정 페이지 숨김
 */
void SettingsPage_hidePage(void);

/**
 * @brief 버튼 입력 처리
 *
 * @param button_id 버튼 ID
 */
void SettingsPage_handleButton(int button_id);

/**
 * @brief 페이지 업데이트
 */
void SettingsPage_update(void);

/**
 * @brief 현재 선택된 메뉴가 Exit인지 확인
 *
 * @return true Exit 메뉴 선택, false 그 외
 */
bool SettingsPage_isExitSelected(void);

/**
 * @brief 현재 팩토리 리셋 확인 페이지 상태인지 확인
 *
 * @return true 팩토리 리셋 확인 페이지, false 그 외
 */
bool SettingsPage_isInFactoryResetConfirm(void);

/**
 * @brief 현재 Camera ID 변경 중인지 확인 (RX 전용)
 *
 * @return true Camera ID 변경 중, false 그 외
 */
bool SettingsPage_isInCameraIdChange(void);

/**
 * @brief 롱프레스 시작 처리
 *
 * @param button_id 버튼 ID
 */
void SettingsPage_handleLongPress(int button_id);

/**
 * @brief 롱프레스 해제 처리
 *
 * @param button_id 버튼 ID
 */
void SettingsPage_handleLongPressRelease(int button_id);


#ifdef __cplusplus
}
#endif

#endif // SETTINGS_PAGE_H