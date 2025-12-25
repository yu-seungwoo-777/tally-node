/**
 * @file TallyPage.h
 * @brief Tally 상태 페이지
 */

#ifndef TALLY_PAGE_H
#define TALLY_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "DisplayManager.h"

// Tally 타입 전방 선언 (나중에 tally_types.h로 교체)
typedef enum {
    TALLY_STATE_OFF = 0,
    TALLY_STATE_SAFE,   // 안전 상태
    TALLY_STATE_PVW,
    TALLY_STATE_PGM
} tally_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TallyPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool tally_page_init(void);

/**
 * @brief Tally 상태 설정
 * @param channel 채널 번호
 * @param state Tally 상태
 */
void tally_page_set_state(uint8_t channel, tally_state_t state);

/**
 * @brief 프로그램 이름 설정
 * @param name 프로그램 이름 (최대 20자)
 */
void tally_page_set_program_name(const char* name);

/**
 * @brief 연결 상태 설정
 * @param connected true = 연결됨, false = 연결 안됨
 */
void tally_page_set_connection(bool connected);

/**
 * @brief 채널 번호 설정
 * @param channel 채널 번호 (1-999)
 */
void tally_page_set_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif // TALLY_PAGE_H
