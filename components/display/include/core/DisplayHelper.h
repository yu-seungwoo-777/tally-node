/**
 * @file DisplayHelper.h
 * @brief Display Helper Functions for Pages
 *
 * 페이지들에서 공통으로 사용하는 디스플레이 헬퍼 함수
 */

#ifndef DISPLAY_HELPER_H
#define DISPLAY_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// U8g2 헤더 포함
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 현재 U8g2 인스턴스 가져오기
 *
 * @return u8g2_t* U8g2 인스턴스 포인터 (실패 시 NULL)
 */
u8g2_t* DisplayHelper_getU8g2(void);

/**
 * @brief 디스플레이 버퍼 지우기
 */
void DisplayHelper_clearBuffer(void);

/**
 * @brief 디스플레이 버퍼 전송
 */
void DisplayHelper_sendBuffer(void);

/**
 * @brief 전원 설정
 *
 * @param on true: 켜기, false: 끄기
 */
void DisplayHelper_setPower(bool on);

#ifdef __cplusplus
}
#endif

// 편의 매크로
#define GET_U8G2() DisplayHelper_getU8g2()
#define U8G2_AVAILABLE() (DisplayHelper_getU8g2() != NULL)

#endif // DISPLAY_HELPER_H