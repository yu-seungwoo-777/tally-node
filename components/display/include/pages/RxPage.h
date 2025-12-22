/**
 * @file RxPage.h
 * @brief TALLY-NODE RX Mode Page
 *
 * RX 모드에서 표시되는 페이지 관리
 * - RX1, RX2 채널 표시
 * - 중앙 정렬된 레이아웃
 */

#ifndef RX_PAGE_H
#define RX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RX 페이지 초기화
 *
 * @return esp_err_t ESP_OK 성공, 그 외 에러
 */
esp_err_t RxPage_init(void);

/**
 * @brief RX 페이지 표시
 */
void RxPage_showPage(void);

/**
 * @brief RX 페이지 숨기기
 */
void RxPage_hidePage(void);

/**
 * @brief RX1 상태 업데이트
 *
 * @param active true: 활성, false: 비활성
 */
void RxPage_setRx1(bool active);

/**
 * @brief RX2 상태 업데이트
 *
 * @param active true: 활성, false: 비활성
 */
void RxPage_setRx2(bool active);

/**
 * @brief 즉각 업데이트를 위한 강제 업데이트
 */
void RxPage_forceUpdate(void);

/**
 * @brief 페이지 전환 (1: Tally, 2: System Info)
 *
 * @param page 1 또는 2
 */
void RxPage_switchPage(uint8_t page);

/**
 * @brief 현재 페이지 번호 조회
 *
 * @return 1 또는 2
 */
uint8_t RxPage_getCurrentPage(void);

#ifdef __cplusplus
}
#endif

#endif // RX_PAGE_H