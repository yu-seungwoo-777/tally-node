/**
 * @file TallyPage.cpp
 * @brief Tally 상태 페이지 구현
 */

#include "TallyPage.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "TallyPage";

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    uint8_t channel;
    tally_state_t state;
    char program_name[21];  // 최대 20자 + null
    bool connected;
} s_tally_page = {
    .channel = 1,
    .state = TALLY_STATE_SAFE,
    .program_name = "No Program",
    .connected = false,
};

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "TallyPage 초기화");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    // 연결 상태 표시 (상단 표시줄)
    if (s_tally_page.connected) {
        u8g2_DrawBox(u8g2, 0, 0, 128, 3);  // 연결됨: 상단 표시줄
    } else {
        u8g2_DrawFrame(u8g2, 0, 0, 128, 3); // 연결 안됨: 빈 테두리
    }

    // 채널 번호 (왼쪽 상단, 큰 폰트)
    char ch_str[8];
    snprintf(ch_str, sizeof(ch_str), "CH%02d", s_tally_page.channel);
    u8g2_SetFont(u8g2, u8g2_font_profont29_mn);
    u8g2_DrawStr(u8g2, 4, 38, ch_str);

    // Tally 상태 표시 (오른쪽 상단)
    const char* status_str = "SAFE";
    if (s_tally_page.state == TALLY_STATE_PVW) {
        status_str = "PREVIEW";
    } else if (s_tally_page.state == TALLY_STATE_PGM) {
        status_str = "PROGRAM";
    }

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 80, 15, status_str);

    // 프로그램 이름 (하단)
    u8g2_DrawStr(u8g2, 4, 52, s_tally_page.program_name);

    // Tally 상태에 따른 하단 표시줄
    int bar_y = 58;
    if (s_tally_page.state == TALLY_STATE_PGM) {
        // PROGRAM: 빨간색 하단 표시줄 (실제로는 채워진 사각형)
        u8g2_DrawBox(u8g2, 0, bar_y, 128, 6);
    } else if (s_tally_page.state == TALLY_STATE_PVW) {
        // PREVIEW: 중간 하단 표시줄 (양쪽 테두리)
        u8g2_DrawFrame(u8g2, 0, bar_y, 128, 6);
    }
    // SAFE: 하단 표시줄 없음
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "TallyPage 진입");
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "TallyPage 퇴장");
}

// ============================================================================
// 페이지 인터페이스 정의
// ============================================================================

static const display_page_interface_t s_tally_page_interface = {
    .id = PAGE_TALLY,
    .name = "Tally",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
};

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool tally_page_init(void)
{
    return display_manager_register_page(&s_tally_page_interface);
}

extern "C" void tally_page_set_state(uint8_t channel, tally_state_t state)
{
    s_tally_page.channel = channel;
    s_tally_page.state = state;
}

extern "C" void tally_page_set_program_name(const char* name)
{
    if (name != nullptr) {
        strncpy(s_tally_page.program_name, name, sizeof(s_tally_page.program_name) - 1);
        s_tally_page.program_name[sizeof(s_tally_page.program_name) - 1] = '\0';
    }
}

extern "C" void tally_page_set_connection(bool connected)
{
    s_tally_page.connected = connected;
}

extern "C" void tally_page_set_channel(uint8_t channel)
{
    s_tally_page.channel = channel;
}
