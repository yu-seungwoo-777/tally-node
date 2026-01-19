/**
 * @file BatteryEmptyPage.cpp
 * @brief 배터리 비움 페이지 구현
 *
 * 배터리 잔량이 0%일 때 표시되는 경고 화면입니다.
 * TX/RX 공통으로 사용합니다.
 *
 * 레이아웃:
 * - 상단: "BATTERY EMPTY" 메시지
 * - 중앙: 대형 배터리 아이콘 (카운트다운 포함)
 * - 하단: "CHARGE REQUIRED" 또는 "SLEEP IN Xs" 메시지
 */

#include "BatteryEmptyPage.h"
#include "DisplayManager.h"
#include "app_types.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "02_BatteryEmptyPage";

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    bool is_empty;           // 배터리 비움 상태 플래그
    display_page_t prev_page; // 이전 페이지 (복귀용)
    uint8_t blink_counter;    // 깜빡임 카운터 (0~1)
    bool timer_completed;     // 카운트다운 완료 플래그
} s_battery_empty = {
    .is_empty = false,
    .prev_page = PAGE_NONE,
    .blink_counter = 0,
    .timer_completed = false,
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 대형 배터리 아이콘 그리기 (빈 상태)
 * @param u8g2 u8g2 핸들
 * @param cx 중앙 X 좌표
 * @param cy 중앙 Y 좌표
 * @param w 배터리 본체 너비
 * @param h 배터리 본체 높이
 */
static void draw_empty_battery_icon(u8g2_t* u8g2, int cx, int cy, int w, int h)
{
    const int tip_w = 6;   // 배터리 돌기 너비
    const int tip_h = 8;   // 배터리 돌기 높이

    // 배터리 본체 (중앙 기준)
    int body_x = cx - w / 2;
    int body_y = cy - h / 2;

    // 배터리 돌기 (오른쪽 중앙)
    int tip_x = body_x + w;
    int tip_y = cy - tip_h / 2;

    // 배터리 본체 테두리 (굵게)
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, body_x, body_y, w, h);
    u8g2_DrawFrame(u8g2, body_x + 1, body_y + 1, w - 2, h - 2);

    // 배터리 돌기 (오른쪽)
    u8g2_DrawFrame(u8g2, tip_x, tip_y, tip_w, tip_h);
    u8g2_DrawFrame(u8g2, tip_x + 1, tip_y + 1, tip_w - 2, tip_h - 2);
}

/**
 * @brief 전압 표시 (배터리 안쪽)
 * @param u8g2 u8g2 핸들
 * @param x 중앙 X 좌표
 * @param y Y 좌표
 */
static void draw_voltage_center(u8g2_t* u8g2, int x, int y)
{
    float voltage = display_manager_get_voltage();

    char buf[8];
    snprintf(buf, sizeof(buf), "%.1fV", voltage);

    // 큰 폰트
    u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
    int width = u8g2_GetStrWidth(u8g2, buf);

    u8g2_DrawStr(u8g2, x - width / 2, y, buf);
}

/**
 * @brief 카운트다운 표시 (배터리 안쪽, 전압 위)
 * @param u8g2 u8g2 핸들
 * @param seconds 남은 시간
 * @param x 중앙 X 좌표
 * @param y Y 좌표
 */
static void draw_countdown_center(u8g2_t* u8g2, uint8_t seconds, int x, int y)
{
    if (seconds == 0 || seconds > 99) {
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", seconds);

    // 큰 폰트
    u8g2_SetFont(u8g2, u8g2_font_profont29_mf);
    int width = u8g2_GetStrWidth(u8g2, buf);

    // 깜빡이는 효과 (마지막 3초는 빠르게 깜빡임)
    if (seconds <= 3) {
        // 3초 이하일 때는 반으로 깜빡임
        if (s_battery_empty.blink_counter == 0) {
            u8g2_SetDrawColor(u8g2, 0);  // 반전
        }
    } else if (seconds <= 5) {
        // 5초 이하일 때는 느리게 깜빡임
        if (s_battery_empty.blink_counter == 1) {
            u8g2_SetDrawColor(u8g2, 0);  // 반전
        }
    }

    u8g2_DrawStr(u8g2, x - width / 2, y, buf);
    u8g2_SetDrawColor(u8g2, 1);  // 복원
}

/**
 * @brief 배터리 비움 화면 그리기
 */
static void draw_battery_empty_screen(u8g2_t* u8g2)
{
    const int screen_w = 128;

    uint8_t countdown = display_manager_get_deep_sleep_countdown();
    bool has_countdown = (countdown > 0 && countdown <= 10);

    // =====================================================
    // 상단: "BATTERY EMPTY" 메시지
    // =====================================================
    u8g2_SetFont(u8g2, u8g2_font_profont12_mf);
    const char* title = "BATTERY EMPTY";
    int title_width = u8g2_GetStrWidth(u8g2, title);

    // 깜빡이는 타이틀 (긴박한 상태 강조)
    if (s_battery_empty.blink_counter == 0 && countdown <= 3) {
        u8g2_SetDrawColor(u8g2, 0);  // 반전
    }
    u8g2_DrawStr(u8g2, (screen_w - title_width) / 2, 12, title);
    u8g2_SetDrawColor(u8g2, 1);  // 복원

    // =====================================================
    // 중앙: 대형 배터리 아이콘
    // =====================================================
    const int bat_cx = screen_w / 2;  // 중앙 X
    const int bat_cy = 32;            // 중앙 Y (화면 높이 64의 중앙)
    const int bat_w = 80;             // 배터리 너비
    const int bat_h = 28;             // 배터리 높이

    draw_empty_battery_icon(u8g2, bat_cx, bat_cy, bat_w, bat_h);

    // 3-state 로직:
    // 1. 초기 상태 (timer_completed=false, countdown=0): 아무것도 표시 안함
    // 2. 카운트다운 중 (countdown>0): 숫자 표시
    // 3. 카운트다운 완료 (timer_completed=true, countdown=0): 전압 표시
    if (has_countdown) {
        draw_countdown_center(u8g2, countdown, bat_cx, bat_cy + 10);
    } else if (s_battery_empty.timer_completed) {
        draw_voltage_center(u8g2, bat_cx, bat_cy + 7);
    }
    // 초기 상태에서는 아무것도 표시하지 않음 (빈 배터리 아이콘만)

    // =====================================================
    // 하단: 메시지
    // =====================================================
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    if (has_countdown) {
        // 카운트다운 중: "SLEEP IN Xs"
        char msg[32];
        snprintf(msg, sizeof(msg), "SLEEP IN %ds", countdown);
        int msg_width = u8g2_GetStrWidth(u8g2, msg);
        u8g2_DrawStr(u8g2, (screen_w - msg_width) / 2, 60, msg);
    } else {
        // 충전 후 재부팅 안내
        const char* msg = "CHARGE & REBOOT";
        int msg_width = u8g2_GetStrWidth(u8g2, msg);
        u8g2_DrawStr(u8g2, (screen_w - msg_width) / 2, 60, msg);
    }
}

/**
 * @brief 깜빡임 카운터 토글 (1초마다 호출)
 */
static void toggle_blink(void)
{
    s_battery_empty.blink_counter = (s_battery_empty.blink_counter + 1) & 1;
}

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "BatteryEmptyPage initialized");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    draw_battery_empty_screen(u8g2);
}

/**
 * @brief 1초 간격 타이머 콜백 (카운트다운 및 깜빡임)
 */
static void page_timer_tick(void)
{
    toggle_blink();
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGW(TAG, "BatteryEmptyPage entered - Battery is empty!");
    s_battery_empty.blink_counter = 0;
    s_battery_empty.timer_completed = false;  // 카운트다운 완료 플래그 리셋
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGI(TAG, "BatteryEmptyPage exited - Battery charged");
    s_battery_empty.blink_counter = 0;
}

// ============================================================================
// 페이지 인터페이스 정의
// ============================================================================

static const display_page_interface_t s_battery_empty_page_interface = {
    .id = PAGE_BATTERY_EMPTY,
    .name = "BatteryEmpty",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
    .timer_tick = page_timer_tick,
};

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool battery_empty_page_init(void)
{
    return display_manager_register_page(&s_battery_empty_page_interface);
}

extern "C" void battery_empty_page_set_empty(bool empty)
{
    s_battery_empty.is_empty = empty;

    if (empty) {
        display_page_t current = display_manager_get_current_page();
        if (current != PAGE_BATTERY_EMPTY) {
            s_battery_empty.prev_page = current;
            display_manager_set_page(PAGE_BATTERY_EMPTY);
            T_LOGW(TAG, "Battery empty detected - Showing empty page");
        }
    } else {
        if (s_battery_empty.prev_page != PAGE_NONE && s_battery_empty.prev_page != PAGE_BATTERY_EMPTY) {
            display_manager_set_page(s_battery_empty.prev_page);
        } else {
#ifdef DEVICE_MODE_TX
            display_manager_set_page(PAGE_TX);
#elif defined(DEVICE_MODE_RX)
            display_manager_set_page(PAGE_RX);
#endif
        }
        T_LOGI(TAG, "Battery recovered - Restoring previous page");
    }
}

extern "C" bool battery_empty_page_is_empty(void)
{
    return s_battery_empty.is_empty;
}

extern "C" void battery_empty_page_show(void)
{
    s_battery_empty.prev_page = display_manager_get_current_page();
    display_manager_set_page(PAGE_BATTERY_EMPTY);
    s_battery_empty.is_empty = true;
}

extern "C" void battery_empty_page_hide(void)
{
    s_battery_empty.is_empty = false;
    if (s_battery_empty.prev_page != PAGE_NONE && s_battery_empty.prev_page != PAGE_BATTERY_EMPTY) {
        display_manager_set_page(s_battery_empty.prev_page);
    } else {
#ifdef DEVICE_MODE_TX
        display_manager_set_page(PAGE_TX);
#elif defined(DEVICE_MODE_RX)
        display_manager_set_page(PAGE_RX);
#endif
    }
}

extern "C" void battery_empty_page_set_timer_completed(bool completed)
{
    s_battery_empty.timer_completed = completed;
}
