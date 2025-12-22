/**
 * @file BootScreen.h
 * @brief TALLY-NODE Boot Screen Manager
 *
 * TALLY-NODE 전용 부팅 화면 관리
 * - 프로페셔셔널 디자인의 부팅 화면
 * - 중앙 정렬된 정보 표시
 * - 진행률 및 상태 표시
 */

#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 부트 스크린 초기화
 *
 * @return esp_err_t ESP_OK 성공, 그 외 에러
 */
esp_err_t BootScreen_init(void);

/**
 * @brief 초기 부트 화면 표시
 */
void BootScreen_showBootScreen(void);

/**
 * @brief 부트 메시지 표시
 *
 * @param message 메시지
 * @param progress 진행률 (0-100)
 * @param delay_ms 표시 시간 (밀리초)
 */
void BootScreen_showBootMessage(const char* message, int progress, int delay_ms);

/**
 * @brief 부트 완료 표시
 *
 * @param success true: 성공, false: 실패
 * @param message 최종 메시지
 */
void BootScreen_bootComplete(bool success, const char* message);

#ifdef __cplusplus
}
#endif

#endif // BOOT_SCREEN_H