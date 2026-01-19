/**
 * @file BatteryEmptyPage.h
 * @brief 배터리 비움 페이지 (배터리 0% 경고 화면)
 *
 * TX/RX 공통으로 사용하는 배터리 비움 상태 페이지입니다.
 * 배터리 잔량이 0%일 때 자동으로 표시됩니다.
 */

#ifndef BATTERY_EMPTY_PAGE_H
#define BATTERY_EMPTY_PAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BatteryEmptyPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool battery_empty_page_init(void);

/**
 * @brief 배터리 비움 상태 설정 (페이지 전환)
 * @param empty true: 배터리 비움 상태, false: 정상 상태
 */
void battery_empty_page_set_empty(bool empty);

/**
 * @brief 배터리 비움 상태 확인
 * @return true: 배터리 비움 상태, false: 정상 상태
 */
bool battery_empty_page_is_empty(void);

/**
 * @brief 배터리 비움 페이지로 강제 전환
 */
void battery_empty_page_show(void);

/**
 * @brief 배터리 비움 상태 해제 (이전 페이지로 복귀)
 */
void battery_empty_page_hide(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_EMPTY_PAGE_H
