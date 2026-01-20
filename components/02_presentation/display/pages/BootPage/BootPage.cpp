/**
 * @file BootPage.cpp
 * @brief 부팅 화면 페이지 구현
 */

#include "BootPage.h"
#include "DisplayManager.h"
#include "app_types.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "02_BootPage";

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
    T_LOGI(TAG, "BootPage initialized");
}

/**
 * @brief 프로페셔널 박스 그리기 (examples/1 참고)
 */
static void draw_professional_box(u8g2_t* u8g2)
{
    // 프로페셔널 박스 (가운데 정렬)
    const int box_width = 124;  // 너비
    const int box_height = 34;  // 2줄 텍스트를 위한 높이
    const int box_x = (128 - box_width) / 2;  // 가운데 정렬
    const int box_y = 2;

    // 박스 그리기 (두꺼운 테두리, +2 간격)
    u8g2_DrawFrame(u8g2, box_x, box_y, box_width, box_height);
    u8g2_DrawFrame(u8g2, box_x + 2, box_y + 2, box_width - 4, box_height - 4);

    // 타이틀과 버전 (1줄, 중앙 정렬)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char title_version[32];
    snprintf(title_version, sizeof(title_version), "TALLY-NODE v%s", FIRMWARE_VERSION);
    int title_width = u8g2_GetStrWidth(u8g2, title_version);
    int title_x = box_x + (box_width - title_width) / 2;
    u8g2_DrawStr(u8g2, title_x, box_y + 14, title_version);

    // 모드 정보 (2줄, 중앙 정렬)
    char mode_str[32];
#ifdef DEVICE_MODE_TX
    snprintf(mode_str, sizeof(mode_str), "MODE: TX (868MHz)");
#else
    snprintf(mode_str, sizeof(mode_str), "MODE: RX (868MHz)");
#endif
    int mode_width = u8g2_GetStrWidth(u8g2, mode_str);
    int mode_x = box_x + (box_width - mode_width) / 2;
    u8g2_DrawStr(u8g2, mode_x, box_y + 26, mode_str);
}

/**
 * @brief 렌더링 (examples/1 BootScreen 참고)
 */
static void page_render(u8g2_t* u8g2)
{
    // 화면 지우기는 DisplayManager에서 수행됨

    // 프로페셔널 박스
    draw_professional_box(u8g2);

    // 진행 문구와 퍼센트 표시 (1줄, 중앙 정렬)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char percent_text[12];
    snprintf(percent_text, sizeof(percent_text), "%d%%", s_boot_page.progress);

    // 문구와 퍼센트 결합
    char combined_text[80];
    snprintf(combined_text, sizeof(combined_text), "%s %s", s_boot_page.message, percent_text);
    int msg_width = u8g2_GetStrWidth(u8g2, combined_text);
    int msg_x = (128 - msg_width) / 2;
    u8g2_DrawStr(u8g2, msg_x, 50, combined_text);

    // 프로그레스바 (전체 너비)
    const int bar_width = 112;  // 전체 너비에서 좌우 여백 8px씩
    const int bar_height = 6;
    const int bar_x = 8;
    const int bar_y = 56;

    // 프로그레스바 배경
    u8g2_DrawFrame(u8g2, bar_x, bar_y, bar_width, bar_height);

    // 프로그레스바 채우기
    if (s_boot_page.progress > 0) {
        int fill_width = (bar_width * s_boot_page.progress) / 100;
        if (fill_width > 0) {
            u8g2_DrawBox(u8g2, bar_x, bar_y, fill_width, bar_height);
        }
    }
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "BootPage entered");
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "BootPage exited");
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
    .timer_tick = nullptr,
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
