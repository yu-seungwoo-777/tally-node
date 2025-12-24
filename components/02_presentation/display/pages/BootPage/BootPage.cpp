/**
 * @file BootPage.cpp
 * @brief 부팅 화면 페이지 구현
 */

#include "BootPage.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "BootPage";

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    char message[32];
    uint8_t progress;
} s_boot_page = {
    .message = "EoRa-S3 Tally Node",
    .progress = 0,
};

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "BootPage 초기화");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    // 화면 지우기는 DisplayManager에서 수행됨

    // 타이틀
    u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
    u8g2_DrawStr(u8g2, 4, 24, "EoRa-S3");

    // 메시지
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 4, 38, s_boot_page.message);

    // 진행률 바
    const int bar_x = 4;
    const int bar_y = 48;
    const int bar_width = 120;
    const int bar_height = 8;

    // 배경 (빈 바)
    u8g2_DrawFrame(u8g2, bar_x, bar_y, bar_width, bar_height);

    // 진행률
    if (s_boot_page.progress > 0) {
        int fill_width = (bar_width * s_boot_page.progress) / 100;
        u8g2_DrawBox(u8g2, bar_x, bar_y, fill_width, bar_height);
    }

    // 진행률 텍스트
    char progress_str[8];
    snprintf(progress_str, sizeof(progress_str), "%d%%", s_boot_page.progress);
    u8g2_DrawStr(u8g2, 4, 62, progress_str);

    // 버전 정보 (오른쪽 하단)
    u8g2_DrawStr(u8g2, 80, 62, "v1.0");
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "BootPage 진입");
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "BootPage 퇴장");
}

// ============================================================================
// 페이지 인터페이스 정의
// ============================================================================

static const display_page_interface_t s_boot_page_interface = {
    .id = PAGE_BOOT,
    .name = "Boot",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
};

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool boot_page_init(void)
{
    // DisplayManager에 페이지 등록
    return display_manager_register_page(&s_boot_page_interface);
}

extern "C" void boot_page_set_message(const char* message)
{
    if (message != nullptr) {
        strncpy(s_boot_page.message, message, sizeof(s_boot_page.message) - 1);
        s_boot_page.message[sizeof(s_boot_page.message) - 1] = '\0';
    }
}

extern "C" void boot_page_set_progress(uint8_t progress)
{
    if (progress > 100) {
        progress = 100;
    }
    s_boot_page.progress = progress;
}
