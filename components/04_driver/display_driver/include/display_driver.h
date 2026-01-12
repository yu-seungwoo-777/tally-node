/**
 * @file DisplayDriver.h
 * @brief 디스플레이 드라이버 (C++)
 *
 * U8g2 기반 SSD1306 OLED 디스플레이 드라이버
 *
 * ============================================================================
 * 디자인 픽셀 위치 기준
 * ============================================================================
 *
 * 화면 크기: 128 x 64 픽셀
 * 좌표계: (0, 0) = 왼쪽 상단
 *
 * 텍스트 그리기 (u8g2_DrawStr):
 *   - x: 왼쪽 기준 (padding 포함)
 *   - y: 베이스라인 기준 (글자가 그려지는 하단 기준선)
 *
 * 예시 (profont11_mf, 높이 11px):
 *   u8g2_DrawStr(u8g2, 4, 10, "첫번째 줄");   // y=10 베이스라인
 *   u8g2_DrawStr(u8g2, 4, 21, "두번째 줄");  // y=21 베이스라인 (11px 간격)
 *   u8g2_DrawStr(u8g2, 4, 32, "세번째 줄");  // y=32 베이스라인
 *
 * 테두리 그리기 (u8g2_DrawFrame):
 *   u8g2_DrawFrame(u8g2, 0, 0, 128, 64);  // 전체 화면 테두리
 *
 * 참고: 텍스트가 테두리와 겹치지 않으려면 x ≥ 4 권장
 * ============================================================================
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 디스플레이 드라이버 초기화
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t display_driver_init(void);

/**
 * @brief 디스플레이 전원 켜기/끄기
 *
 * @param on true: 켜기, false: 끄기
 */
void display_driver_set_power(bool on);

/**
 * @brief 화면 지우기
 */
void display_driver_clear_buffer(void);

/**
 * @brief 버퍼 전송 (화면 업데이트) - 뮤텍스 보호됨
 */
void display_driver_send_buffer(void);

/**
 * @brief 버퍼 전송 (동기) - 뮤텍스 외부에서 호출
 *
 * @note 이미 뮤텍스를 획득한 상태에서 호출
 */
void display_driver_send_buffer_sync(void);

/**
 * @brief 디스플레이 뮤텍스 획득
 *
 * @return ESP_OK 성공, ESP_ERR_TIMEOUT 타임아웃
 */
esp_err_t display_driver_take_mutex(uint32_t timeout_ms);

/**
 * @brief 디스플레이 뮤텍스 해제
 */
void display_driver_give_mutex(void);

/**
 * @brief U8g2 인스턴스 가져오기
 *
 * @return U8g2 구조체 포인터
 */
u8g2_t* display_driver_get_u8g2(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_DRIVER_H
