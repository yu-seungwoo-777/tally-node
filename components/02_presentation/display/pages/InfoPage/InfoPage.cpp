/**
 * @file InfoPage.cpp
 * @brief 정보 표시 페이지 구현
 */

#include "InfoPage.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "InfoPage";

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    char ip[16];           // "xxx.xxx.xxx.xxx"
    uint8_t battery;       // 0-100%
    int16_t rssi;          // dBm
    int8_t snr;            // dB
    bool connected;
    uint32_t uptime;       // seconds
} s_info_page = {
    .ip = "No IP",
    .battery = 0,
    .rssi = -127,
    .snr = 0,
    .connected = false,
    .uptime = 0,
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 초를 시간:분:초로 변환
 */
static void format_uptime(uint32_t seconds, char* buf, size_t buf_size)
{
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    snprintf(buf, buf_size, "%02lu:%02lu:%02lu", hours, minutes, secs);
}

/**
 * @brief 배터리 아이콘 그리기
 */
static void draw_battery_icon(u8g2_t* u8g2, int x, int y, uint8_t percent)
{
    const int w = 20;
    const int h = 8;

    // 배터리 외곽
    u8g2_DrawFrame(u8g2, x, y, w, h);
    // 배터리 헤드
    u8g2_DrawVLine(u8g2, x + w, y + 2, 4);

    // 충전량
    if (percent > 0) {
        int fill_w = (w - 2) * percent / 100;
        if (fill_w > 0) {
            u8g2_DrawBox(u8g2, x + 1, y + 1, fill_w, h - 2);
        }
    }
}

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "InfoPage 초기화");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    char buf[32];
    int y = 12;

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // 타이틀
    u8g2_DrawStr(u8g2, 4, y, "Info");
    y += 2;

    // 구분선
    u8g2_DrawHLine(u8g2, 0, y, 128);
    y += 10;

    // IP 주소
    u8g2_DrawStr(u8g2, 4, y, "IP:");
    u8g2_DrawStr(u8g2, 30, y, s_info_page.ip);
    y += 11;

    // 배터리
    u8g2_DrawStr(u8g2, 4, y, "BAT:");
    draw_battery_icon(u8g2, 30, y - 6, s_info_page.battery);
    snprintf(buf, sizeof(buf), "%d%%", s_info_page.battery);
    u8g2_DrawStr(u8g2, 55, y, buf);
    y += 11;

    // LoRa RSSI
    u8g2_DrawStr(u8g2, 4, y, "RSSI:");
    if (s_info_page.rssi <= -127) {
        u8g2_DrawStr(u8g2, 35, y, "N/A");
    } else {
        snprintf(buf, sizeof(buf), "%ddBm", s_info_page.rssi);
        u8g2_DrawStr(u8g2, 35, y, buf);
    }
    y += 11;

    // LoRa SNR
    u8g2_DrawStr(u8g2, 4, y, "SNR:");
    if (s_info_page.snr == 0 && s_info_page.rssi <= -127) {
        u8g2_DrawStr(u8g2, 30, y, "N/A");
    } else {
        snprintf(buf, sizeof(buf), "%ddB", s_info_page.snr);
        u8g2_DrawStr(u8g2, 30, y, buf);
    }
    y += 11;

    // 연결 상태
    u8g2_DrawStr(u8g2, 4, y, "Link:");
    u8g2_DrawStr(u8g2, 30, y, s_info_page.connected ? "Connected" : "Disconnected");
    y += 11;

    // 업타임
    format_uptime(s_info_page.uptime, buf, sizeof(buf));
    u8g2_DrawStr(u8g2, 4, y, "Up:");
    u8g2_DrawStr(u8g2, 30, y, buf);
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "InfoPage 진입");
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "InfoPage 퇴장");
}

// ============================================================================
// 페이지 인터페이스 정의
// ============================================================================

static const display_page_interface_t s_info_page_interface = {
    .id = PAGE_INFO,
    .name = "Info",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
};

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool info_page_init(void)
{
    return display_manager_register_page(&s_info_page_interface);
}

extern "C" void info_page_set_ip(const char* ip)
{
    if (ip != nullptr) {
        strncpy(s_info_page.ip, ip, sizeof(s_info_page.ip) - 1);
        s_info_page.ip[sizeof(s_info_page.ip) - 1] = '\0';
    }
}

extern "C" void info_page_set_battery(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_info_page.battery = percent;
}

extern "C" void info_page_set_rssi(int16_t rssi)
{
    s_info_page.rssi = rssi;
}

extern "C" void info_page_set_snr(int8_t snr)
{
    s_info_page.snr = snr;
}

extern "C" void info_page_set_connection(bool connected)
{
    s_info_page.connected = connected;
}

extern "C" void info_page_set_uptime(uint32_t seconds)
{
    s_info_page.uptime = seconds;
}
