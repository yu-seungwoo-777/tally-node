/**
 * @file TxPage.cpp
 * @brief TX 모드 페이지 구현 (스위처 연결 상태)
 *
 * 6개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 스위처 정보 (S1, S2 듀얼 모드 지원)
 * - Page 3: AP (이름, 비밀번호, IP)
 * - Page 4: WIFI (SSID, 비밀번호, IP)
 * - Page 5: ETHERNET (IP, 게이트웨이)
 * - Page 6: 시스템 정보 (3x2 테이블)
 */

#include "TxPage.h"
#include "icons.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "02_TxPage";

// ============================================================================
// 내부 상태
// ============================================================================

// Tally 데이터 (Page 1)
static struct {
    uint8_t pgm_channels[20];
    uint8_t pgm_count;
    uint8_t pvw_channels[20];
    uint8_t pvw_count;
} s_tally_data = {
    .pgm_channels = {0},
    .pgm_count = 0,
    .pvw_channels = {0},
    .pvw_count = 0,
};

// 스위처 정보 (Page 2)
static struct {
    bool dual_mode;
    char s1_type[16];
    char s1_ip[32];
    uint16_t s1_port;
    bool s1_connected;
    char s2_type[16];
    char s2_ip[32];
    uint16_t s2_port;
    bool s2_connected;
} s_switcher_data = {
    .dual_mode = false,
    .s1_type = "NONE",
    .s1_ip = "0.0.0.0",
    .s1_port = 0,
    .s1_connected = false,
    .s2_type = "NONE",
    .s2_ip = "0.0.0.0",
    .s2_port = 0,
    .s2_connected = false,
};

// AP 정보 (Page 3)
static struct {
    char ap_name[32];       // AP SSID
    char ap_password[64];   // AP 비밀번호
    char ap_ip[16];        // AP IP
    bool ap_enabled;        // AP 활성화 상태
} s_ap_data = {
    .ap_name = "TallyNode-AP",
    .ap_password = "********",
    .ap_ip = "192.168.4.1",
    .ap_enabled = false,    // 기본 비활성화
};

// WIFI 정보 (Page 4)
static struct {
    char wifi_ssid[32];     // WIFI SSID
    char wifi_password[64]; // WIFI 비밀번호
    char wifi_ip[16];       // WIFI IP
    bool wifi_connected;
} s_wifi_data = {
    .wifi_ssid = "",
    .wifi_password = "********",
    .wifi_ip = "",
    .wifi_connected = false,
};

// ETHERNET 정보 (Page 5)
static struct {
    char eth_ip[16];        // Ethernet IP
    bool eth_dhcp_mode;     // true=DHCP, false=Static
    bool eth_connected;
} s_eth_data = {
    .eth_ip = "",
    .eth_dhcp_mode = true,
    .eth_connected = false,
};

// 시스템 정보 (Page 6)
static struct {
    uint8_t battery_percent;
    float frequency;
    uint8_t sync_word;
    float voltage;
    float temperature;
    char device_id[17];
    uint64_t uptime_sec;
} s_system_data = {
    .battery_percent = 75,
    .frequency = 868.0f,
    .sync_word = 0x12,
    .voltage = 3.7f,
    .temperature = 25.0f,
    .device_id = "????????",
    .uptime_sec = 0,
};

// 현재 페이지 (1: Tally, 2: Switcher, 3: AP, 4: WIFI, 5: ETHERNET, 6: System)
static uint8_t s_current_page = 1;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static void draw_tx_header(u8g2_t* u8g2);
static void draw_tally_page(u8g2_t* u8g2);
static void draw_channel_list(u8g2_t* u8g2, const uint8_t* channels, uint8_t count,
                              int y_pos, int max_width);
static void draw_switcher_page(u8g2_t* u8g2);
static void draw_ap_page(u8g2_t* u8g2);
static void draw_wifi_page(u8g2_t* u8g2);
static void draw_ethernet_page(u8g2_t* u8g2);
static void draw_system_page(u8g2_t* u8g2);

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

static void page_init(void)
{
    T_LOGI(TAG, "TxPage initialized");
}

static void page_render(u8g2_t* u8g2)
{
    switch (s_current_page) {
        case 1:
            draw_tally_page(u8g2);
            break;
        case 2:
            draw_switcher_page(u8g2);
            break;
        case 3:
            draw_ap_page(u8g2);
            break;
        case 4:
            draw_wifi_page(u8g2);
            break;
        case 5:
            draw_ethernet_page(u8g2);
            break;
        case 6:
            draw_system_page(u8g2);
            break;
        default:
            draw_tally_page(u8g2);
            break;
    }
}

static void page_on_enter(void)
{
    T_LOGD(TAG, "TxPage entered (page %d)", s_current_page);
}

static void page_on_exit(void)
{
    T_LOGD(TAG, "TxPage exited");
}

static const display_page_interface_t s_tx_page_interface = {
    .id = PAGE_TX,
    .name = "TX",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
};

// ============================================================================
// 내부 함수 구현
// ============================================================================

/**
 * @brief TX 헤더 그리기 (배터리 + 페이지네이션)
 */
static void draw_tx_header(u8g2_t* u8g2)
{
    uint8_t battery_level = getBatteryLevel(s_system_data.battery_percent);
    drawTallyBatteryIcon(u8g2, 105, 3, battery_level);

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char page_str[8];
    snprintf(page_str, sizeof(page_str), "%d/6", s_current_page);
    u8g2_DrawStr(u8g2, 80, 10, page_str);
}

/**
 * @brief 채널 리스트 문자열 생성 (가운데 정렬 + 생략)
 */
static void draw_channel_list(u8g2_t* u8g2, const uint8_t* channels, uint8_t count,
                               int y_pos, int max_width)
{
    u8g2_SetFont(u8g2, u8g2_font_profont22_mf);

    if (count == 0) {
        // 채널 없음
        const char* empty = "---";
        int width = u8g2_GetStrWidth(u8g2, empty);
        u8g2_DrawStr(u8g2, (max_width - width) / 2, y_pos, empty);
        return;
    }

    // 전체 문자열 생성 (예: "1,2,3,4,5")
    char full_str[64];
    int offset = 0;
    for (uint8_t i = 0; i < count && i < 20; i++) {
        if (i > 0) {
            offset += snprintf(full_str + offset, sizeof(full_str) - offset, ",");
        }
        offset += snprintf(full_str + offset, sizeof(full_str) - offset, "%d", channels[i]);
    }

    // 전체 너비 계산
    int full_width = u8g2_GetStrWidth(u8g2, full_str);

    if (full_width <= max_width) {
        // 가운데 정렬로 그리기
        u8g2_DrawStr(u8g2, (max_width - full_width) / 2, y_pos, full_str);
    } else {
        // 너무 길면 생략 (...) 처리
        const char ellipsis[] = "...";
        u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
        int ellipsis_width = u8g2_GetStrWidth(u8g2, ellipsis);

        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);

        // 가능한 많이 표시하고 나머지는 ...으로
        char truncated[64];
        int trunc_offset = 0;
        int trunc_width = 0;

        for (uint8_t i = 0; i < count && i < 20; i++) {
            char num_str[8];
            int len = snprintf(num_str, sizeof(num_str), "%d%s",
                              channels[i], (i < count - 1) ? "," : "");
            int num_width = u8g2_GetStrWidth(u8g2, num_str);

            if (trunc_width + num_width + ellipsis_width > max_width) {
                break;
            }

            strncpy(truncated + trunc_offset, num_str, sizeof(truncated) - trunc_offset);
            trunc_offset += len;
            trunc_width += num_width;
        }

        if (trunc_offset > 0) {
            truncated[trunc_offset] = '\0';
            int display_width = trunc_width + ellipsis_width;
            if (display_width > max_width) {
                display_width = max_width;
            }
            u8g2_DrawStr(u8g2, (max_width - display_width) / 2, y_pos, truncated);

            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            u8g2_DrawStr(u8g2, (max_width - display_width) / 2 + trunc_width, y_pos, ellipsis);
        }
    }
}

/**
 * @brief Tally 페이지 그리기 (Page 1)
 */
static void draw_tally_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더: TALLY
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "TALLY");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // 화면 절반 나누기
    u8g2_DrawHLine(u8g2, 0, 39, 128);

    // 리스트 영역 너비: 전체 128px - 라벨 영역(약23px) - 여백(5px) = 100px
    const int list_width = 100;

    // PGM 영역 (위쪽 절반)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 26, "PGM");
    draw_channel_list(u8g2, s_tally_data.pgm_channels, s_tally_data.pgm_count, 34, list_width);

    // PVW 영역 (아래쪽 절반)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 51, "PVW");
    draw_channel_list(u8g2, s_tally_data.pvw_channels, s_tally_data.pvw_count, 59, list_width);
}

/**
 * @brief 스위처 페이지 그리기 (Page 2)
 */
static void draw_switcher_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더: MODE: SINGLE / DUAL
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "MODE:");
    const char* mode_str = s_switcher_data.dual_mode ? "DUAL" : "SINGLE";
    u8g2_DrawStr(u8g2, 35, 10, mode_str);

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // S1 (y=28)
    u8g2_DrawStr(u8g2, 2, 28, "S1:");
    u8g2_DrawStr(u8g2, 25, 28, s_switcher_data.s1_type);
    int s1_end = 25 + u8g2_GetStrWidth(u8g2, s_switcher_data.s1_type) + 5;
    const char* s1_status = s_switcher_data.s1_connected ? "[OK]" : "[FAIL]";
    u8g2_DrawStr(u8g2, s1_end, 28, s1_status);

    // S1 IP (y=39)
    u8g2_DrawStr(u8g2, 2, 39, "S1 IP:");
    if (strlen(s_switcher_data.s1_ip) > 0) {
        u8g2_DrawStr(u8g2, 40, 39, s_switcher_data.s1_ip);
    } else {
        u8g2_DrawStr(u8g2, 40, 39, "---");
    }

    // S2 (듀얼모드일 때만 표시)
    if (s_switcher_data.dual_mode) {
        // S2 (y=50)
        u8g2_DrawStr(u8g2, 2, 50, "S2:");
        u8g2_DrawStr(u8g2, 25, 50, s_switcher_data.s2_type);
        int s2_end = 25 + u8g2_GetStrWidth(u8g2, s_switcher_data.s2_type) + 5;
        const char* s2_status = s_switcher_data.s2_connected ? "[OK]" : "[FAIL]";
        u8g2_DrawStr(u8g2, s2_end, 50, s2_status);

        // S2 IP (y=61)
        u8g2_DrawStr(u8g2, 2, 61, "S2 IP:");
        if (strlen(s_switcher_data.s2_ip) > 0) {
            u8g2_DrawStr(u8g2, 40, 61, s_switcher_data.s2_ip);
        } else {
            u8g2_DrawStr(u8g2, 40, 61, "---");
        }
    }
}

/**
 * @brief AP 페이지 그리기 (Page 3)
 */
static void draw_ap_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "AP");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // SSID
    u8g2_DrawStr(u8g2, 2, 28, "SSID:");

    // SSID 표시 (자르기)
    char ssid_short[16];
    strncpy(ssid_short, s_ap_data.ap_name, 16);
    ssid_short[15] = '\0';
    u8g2_DrawStr(u8g2, 35, 28, ssid_short);

    // IP
    u8g2_DrawStr(u8g2, 2, 39, "IP:");
    u8g2_DrawStr(u8g2, 25, 39, s_ap_data.ap_ip);

    // 상태 표시
    if (!s_ap_data.ap_enabled) {
        u8g2_DrawStr(u8g2, 2, 61, "DISABLED");
    } else {
        u8g2_DrawStr(u8g2, 2, 61, "ACTIVE");
    }
}

/**
 * @brief WIFI 페이지 그리기 (Page 4)
 */
static void draw_wifi_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "WIFI");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // SSID
    u8g2_DrawStr(u8g2, 2, 28, "SSID:");

    // SSID 표시 (연결 상태와 무관하게 설정값 표시)
    char ssid_short[16];
    if (strlen(s_wifi_data.wifi_ssid) > 0) {
        strncpy(ssid_short, s_wifi_data.wifi_ssid, 16);
        ssid_short[15] = '\0';
        u8g2_DrawStr(u8g2, 35, 28, ssid_short);
    } else {
        u8g2_DrawStr(u8g2, 35, 28, "---");
    }

    // IP
    u8g2_DrawStr(u8g2, 2, 39, "IP:");
    if (s_wifi_data.wifi_connected && strlen(s_wifi_data.wifi_ip) > 0) {
        u8g2_DrawStr(u8g2, 25, 39, s_wifi_data.wifi_ip);
    } else {
        u8g2_DrawStr(u8g2, 25, 39, "---");
    }

    // 상태 표시
    if (s_wifi_data.wifi_connected) {
        u8g2_DrawStr(u8g2, 2, 61, "CONNECTED");
    } else {
        u8g2_DrawStr(u8g2, 2, 61, "DISCONNECTED");
    }
}

/**
 * @brief ETHERNET 페이지 그리기 (Page 5)
 */
static void draw_ethernet_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "ETHERNET");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // IP
    u8g2_DrawStr(u8g2, 2, 28, "IP:");
    if (s_eth_data.eth_connected && strlen(s_eth_data.eth_ip) > 0) {
        u8g2_DrawStr(u8g2, 25, 28, s_eth_data.eth_ip);
    } else {
        u8g2_DrawStr(u8g2, 25, 28, "---");
    }

    // 모드 표시
    u8g2_DrawStr(u8g2, 2, 39, s_eth_data.eth_dhcp_mode ? "DHCP" : "STATIC");

    // 상태 표시
    if (s_eth_data.eth_connected) {
        u8g2_DrawStr(u8g2, 2, 61, "LINK UP");
    } else {
        u8g2_DrawStr(u8g2, 2, 61, "LINK DOWN");
    }
}

/**
 * @brief 시스템 정보 페이지 그리기 (Page 6)
 */
static void draw_system_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "SYSTEM");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // FREQ
    u8g2_DrawStr(u8g2, 2, 28, "FREQ:");
    char freq_str[16];
    snprintf(freq_str, sizeof(freq_str), "%.1f MHz", s_system_data.frequency);
    u8g2_DrawStr(u8g2, 35, 28, freq_str);

    // SYNC
    u8g2_DrawStr(u8g2, 2, 39, "SYNC:");
    char sync_str[8];
    snprintf(sync_str, sizeof(sync_str), "0x%02X", s_system_data.sync_word);
    u8g2_DrawStr(u8g2, 35, 39, sync_str);

    // VOLTAGE
    u8g2_DrawStr(u8g2, 2, 50, "VOLTAGE:");
    char volt_str[10];
    snprintf(volt_str, sizeof(volt_str), "%.2f V", s_system_data.voltage);
    u8g2_DrawStr(u8g2, 55, 50, volt_str);

    // TEMP
    u8g2_DrawStr(u8g2, 2, 61, "TEMP:");
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f C", s_system_data.temperature);
    u8g2_DrawStr(u8g2, 35, 61, temp_str);

    // Device ID (오른쪽 정렬)
    int id_width = u8g2_GetStrWidth(u8g2, s_system_data.device_id);
    u8g2_DrawStr(u8g2, 126 - id_width, 61, s_system_data.device_id);
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool tx_page_init(void)
{
    return display_manager_register_page(&s_tx_page_interface);
}

// ========== Tally 정보 (Page 1) ==========

extern "C" void tx_page_set_pgm_channels(const uint8_t* channels, uint8_t count)
{
    s_tally_data.pgm_count = (count > 20) ? 20 : count;
    if (channels != nullptr && s_tally_data.pgm_count > 0) {
        memcpy(s_tally_data.pgm_channels, channels,
               s_tally_data.pgm_count * sizeof(uint8_t));
    }
}

extern "C" void tx_page_set_pvw_channels(const uint8_t* channels, uint8_t count)
{
    s_tally_data.pvw_count = (count > 20) ? 20 : count;
    if (channels != nullptr && s_tally_data.pvw_count > 0) {
        memcpy(s_tally_data.pvw_channels, channels,
               s_tally_data.pvw_count * sizeof(uint8_t));
    }
}

// ========== 스위처 정보 (Page 2) ==========

extern "C" void tx_page_set_dual_mode(bool dual_mode)
{
    s_switcher_data.dual_mode = dual_mode;
}

extern "C" void tx_page_set_s1(const char* type, const char* ip, uint16_t port, bool connected)
{
    if (type != nullptr) {
        strncpy(s_switcher_data.s1_type, type, sizeof(s_switcher_data.s1_type) - 1);
        s_switcher_data.s1_type[sizeof(s_switcher_data.s1_type) - 1] = '\0';
    }
    if (ip != nullptr) {
        strncpy(s_switcher_data.s1_ip, ip, sizeof(s_switcher_data.s1_ip) - 1);
        s_switcher_data.s1_ip[sizeof(s_switcher_data.s1_ip) - 1] = '\0';
    }
    s_switcher_data.s1_port = port;
    s_switcher_data.s1_connected = connected;
}

extern "C" void tx_page_set_s2(const char* type, const char* ip, uint16_t port, bool connected)
{
    if (type != nullptr) {
        strncpy(s_switcher_data.s2_type, type, sizeof(s_switcher_data.s2_type) - 1);
        s_switcher_data.s2_type[sizeof(s_switcher_data.s2_type) - 1] = '\0';
    }
    if (ip != nullptr) {
        strncpy(s_switcher_data.s2_ip, ip, sizeof(s_switcher_data.s2_ip) - 1);
        s_switcher_data.s2_ip[sizeof(s_switcher_data.s2_ip) - 1] = '\0';
    }
    s_switcher_data.s2_port = port;
    s_switcher_data.s2_connected = connected;
}

// ========== AP 정보 (Page 3) ==========

extern "C" void tx_page_set_ap_name(const char* name)
{
    if (name != nullptr) {
        strncpy(s_ap_data.ap_name, name, sizeof(s_ap_data.ap_name) - 1);
        s_ap_data.ap_name[sizeof(s_ap_data.ap_name) - 1] = '\0';
    }
}

extern "C" void tx_page_set_ap_password(const char* password)
{
    if (password != nullptr) {
        strncpy(s_ap_data.ap_password, password, sizeof(s_ap_data.ap_password) - 1);
        s_ap_data.ap_password[sizeof(s_ap_data.ap_password) - 1] = '\0';
    }
}

extern "C" void tx_page_set_ap_ip(const char* ip)
{
    if (ip != nullptr) {
        strncpy(s_ap_data.ap_ip, ip, sizeof(s_ap_data.ap_ip) - 1);
        s_ap_data.ap_ip[sizeof(s_ap_data.ap_ip) - 1] = '\0';
    }
}

extern "C" void tx_page_set_ap_enabled(bool enabled)
{
    s_ap_data.ap_enabled = enabled;
}

// ========== WIFI 정보 (Page 4) ==========

extern "C" void tx_page_set_wifi_ssid(const char* ssid)
{
    if (ssid != nullptr) {
        strncpy(s_wifi_data.wifi_ssid, ssid, sizeof(s_wifi_data.wifi_ssid) - 1);
        s_wifi_data.wifi_ssid[sizeof(s_wifi_data.wifi_ssid) - 1] = '\0';
    }
}

extern "C" void tx_page_set_wifi_password(const char* password)
{
    if (password != nullptr) {
        strncpy(s_wifi_data.wifi_password, password, sizeof(s_wifi_data.wifi_password) - 1);
        s_wifi_data.wifi_password[sizeof(s_wifi_data.wifi_password) - 1] = '\0';
    }
}

extern "C" void tx_page_set_wifi_ip(const char* ip)
{
    if (ip != nullptr) {
        strncpy(s_wifi_data.wifi_ip, ip, sizeof(s_wifi_data.wifi_ip) - 1);
        s_wifi_data.wifi_ip[sizeof(s_wifi_data.wifi_ip) - 1] = '\0';
    }
}

extern "C" void tx_page_set_wifi_connected(bool connected)
{
    s_wifi_data.wifi_connected = connected;
}

// ========== ETHERNET 정보 (Page 5) ==========

extern "C" void tx_page_set_eth_ip(const char* ip)
{
    if (ip != nullptr) {
        strncpy(s_eth_data.eth_ip, ip, sizeof(s_eth_data.eth_ip) - 1);
        s_eth_data.eth_ip[sizeof(s_eth_data.eth_ip) - 1] = '\0';
    }
}

extern "C" void tx_page_set_eth_dhcp_mode(bool dhcp_mode)
{
    s_eth_data.eth_dhcp_mode = dhcp_mode;
}

extern "C" void tx_page_set_eth_connected(bool connected)
{
    s_eth_data.eth_connected = connected;
}

// ========== 시스템 정보 (Page 6) ==========

extern "C" void tx_page_set_battery(uint8_t percent)
{
    s_system_data.battery_percent = (percent > 100) ? 100 : percent;
}

extern "C" void tx_page_set_frequency(float freq_mhz)
{
    s_system_data.frequency = freq_mhz;
}

extern "C" void tx_page_set_sync_word(uint8_t sync_word)
{
    s_system_data.sync_word = sync_word;
}

extern "C" void tx_page_set_voltage(float voltage)
{
    s_system_data.voltage = voltage;
}

extern "C" void tx_page_set_temperature(float temp)
{
    s_system_data.temperature = temp;
}

extern "C" void tx_page_set_device_id(const char* device_id)
{
    if (device_id != nullptr) {
        strncpy(s_system_data.device_id, device_id, 16);
        s_system_data.device_id[16] = '\0';
    }
}

extern "C" void tx_page_set_uptime(uint64_t uptime_sec)
{
    s_system_data.uptime_sec = uptime_sec;
}

// RSSI/SNR (TX 모드에서는 사용하지 않음 - API 호환용)
extern "C" void tx_page_set_rssi(int16_t rssi)
{
    (void)rssi;  // 미사용
}

extern "C" void tx_page_set_snr(float snr)
{
    (void)snr;  // 미사용
}

// ========== 페이지 제어 ==========

extern "C" void tx_page_switch_page(uint8_t page)
{
    if (page >= 1 && page <= 6) {
        s_current_page = page;
    }
}

extern "C" uint8_t tx_page_get_current_page(void)
{
    return s_current_page;
}
