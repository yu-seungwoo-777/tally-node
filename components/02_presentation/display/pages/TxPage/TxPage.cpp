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
#include "TxPageTypes.h"
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
    tx_ap_status_t ap_status;  // AP 활성화 상태 (ENUM)
} s_ap_data = {
    .ap_name = "TallyNode-AP",
    .ap_password = "********",
    .ap_ip = "192.168.4.1",
    .ap_status = TX_AP_STATUS_INACTIVE,  // 기본 비활성화
};

// WIFI 정보 (Page 4)
static struct {
    char wifi_ssid[32];       // WIFI SSID
    char wifi_password[64];   // WIFI 비밀번호
    char wifi_ip[16];         // WIFI IP
    tx_network_status_t wifi_status;  // WiFi 연결 상태 (ENUM)
} s_wifi_data = {
    .wifi_ssid = "",
    .wifi_password = "********",
    .wifi_ip = "",
    .wifi_status = TX_NET_STATUS_NOT_DETECTED,  // 기본 미감지
};

// ETHERNET 정보 (Page 5)
static struct {
    char eth_ip[16];            // Ethernet IP
    bool eth_dhcp_mode;         // true=DHCP, false=Static
    tx_network_status_t eth_status;  // Ethernet 연결 상태 (ENUM)
} s_eth_data = {
    .eth_ip = "",
    .eth_dhcp_mode = true,
    .eth_status = TX_NET_STATUS_NOT_DETECTED,  // 기본 미감지
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
static void draw_hybrid_dashboard_page(u8g2_t* u8g2);
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
            draw_hybrid_dashboard_page(u8g2);
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
            draw_hybrid_dashboard_page(u8g2);
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
    .timer_tick = nullptr,
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
    snprintf(page_str, sizeof(page_str), "%d/%d", s_current_page, TX_PAGE_COUNT);
    u8g2_DrawStr(u8g2, 80, 10, page_str);

    // DASHBOARD 텍스트 (Page 1 전용)
    if (s_current_page == 1) {
        u8g2_DrawStr(u8g2, 2, 10, "DASHBOARD");
    }
}

/**
 * @brief 하이브리드 대시보드 페이지 그리기 (Page 1)
 *
 * 4-Line 2열 레이아웃 (오른쪽 항목 고정 X축 정렬):
 * - Line 1 (y=28): "PGM: 1,2,3,4" | "AP:[V]" (x=80)
 * - Line 2 (y=39): "PVW: 5" | "WiFi:[V]" (x=80)
 * - Line 3 (y=50): "> SINGLE" / "> DUAL" | "ETH:[V]" (x=80)
 * - Line 4 (y=61): 스위처 정보 "1: ATEM" / "2: OBS"
 */
static void draw_hybrid_dashboard_page(u8g2_t* u8g2)
{
    draw_tx_header(u8g2);

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // 2열 구분 세로선 (Line 3까지만, y=18~50, 길이=32)
    const int divider_x = 75;
    u8g2_DrawVLine(u8g2, divider_x, 18, 32);

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // 오른쪽 정렬 X축 고정값
    const int right_align_x = 80;

    // 결과([V]/[X]/[-])는 동일 x축에 표시 (가장 긴 라벨 WiFi: 기준)
    const int status_x = right_align_x + u8g2_GetStrWidth(u8g2, "WiFi:") + 1;

    // Line 1 (y=28): PGM 채널 목록 (최대 3개 + ...) | AP 상태
    char pgm_str[32];
    if (s_tally_data.pgm_count > 0) {
        int offset = 0;
        offset += snprintf(pgm_str, sizeof(pgm_str), "PGM: ");

        // 최대 3개까지만 표시
        uint8_t display_count = (s_tally_data.pgm_count > 3) ? 3 : s_tally_data.pgm_count;
        for (uint8_t i = 0; i < display_count; i++) {
            offset += snprintf(pgm_str + offset, sizeof(pgm_str) - offset,
                              "%s%d", (i > 0) ? "," : "", s_tally_data.pgm_channels[i]);
        }

        // 남은 채널 있으면 .. 추가
        if (s_tally_data.pgm_count > 3) {
            snprintf(pgm_str + offset, sizeof(pgm_str) - offset, "..");
        }
    } else {
        snprintf(pgm_str, sizeof(pgm_str), "PGM:  ---");
    }
    u8g2_DrawStr(u8g2, 2, 28, pgm_str);

    // AP 라벨
    u8g2_DrawStr(u8g2, right_align_x, 28, "AP:");
    // AP 결과 (동일 x축)
    if (s_ap_data.ap_status == TX_AP_STATUS_ACTIVE) {
        u8g2_DrawStr(u8g2, status_x, 28, "[V]");
    } else {
        u8g2_DrawStr(u8g2, status_x, 28, "[X]");
    }

    // Line 2 (y=39): PVW 채널 목록 (최대 3개 + ...) | WiFi 상태
    char pvw_str[32];
    if (s_tally_data.pvw_count > 0) {
        int offset = 0;
        offset += snprintf(pvw_str, sizeof(pvw_str), "PVW: ");

        // 최대 3개까지만 표시
        uint8_t display_count = (s_tally_data.pvw_count > 3) ? 3 : s_tally_data.pvw_count;
        for (uint8_t i = 0; i < display_count; i++) {
            offset += snprintf(pvw_str + offset, sizeof(pvw_str) - offset,
                              "%s%d", (i > 0) ? "," : "", s_tally_data.pvw_channels[i]);
        }

        // 남은 채널 있으면 .. 추가
        if (s_tally_data.pvw_count > 3) {
            snprintf(pvw_str + offset, sizeof(pvw_str) - offset, "..");
        }
    } else {
        snprintf(pvw_str, sizeof(pvw_str), "PVW:  ---");
    }
    u8g2_DrawStr(u8g2, 2, 39, pvw_str);

    // WiFi 라벨
    u8g2_DrawStr(u8g2, right_align_x, 39, "WiFi:");
    // WiFi 결과 (동일 x축)
    switch (s_wifi_data.wifi_status) {
        case TX_NET_STATUS_CONNECTED:
            u8g2_DrawStr(u8g2, status_x, 39, "[V]");
            break;
        case TX_NET_STATUS_DISCONNECTED:
            u8g2_DrawStr(u8g2, status_x, 39, "[-]");
            break;
        case TX_NET_STATUS_NOT_DETECTED:
        default:
            u8g2_DrawStr(u8g2, status_x, 39, "[X]");
            break;
    }

    // Line 3 (y=50): > SINGLE / > DUAL | ETH 상태
    const char* mode_str = s_switcher_data.dual_mode ? "> DUAL" : "> SINGLE";
    u8g2_DrawStr(u8g2, 2, 50, mode_str);

    // ETH 라벨
    u8g2_DrawStr(u8g2, right_align_x, 50, "ETH:");
    // ETH 결과 (동일 x축)
    switch (s_eth_data.eth_status) {
        case TX_NET_STATUS_CONNECTED:
            u8g2_DrawStr(u8g2, status_x, 50, "[V]");
            break;
        case TX_NET_STATUS_DISCONNECTED:
            u8g2_DrawStr(u8g2, status_x, 50, "[-]");
            break;
        case TX_NET_STATUS_NOT_DETECTED:
        default:
            u8g2_DrawStr(u8g2, status_x, 50, "[X]");
            break;
    }

    // Line 4 (y=61): 스위처 정보
    int line4_x = 2;

    // 스위처 정보 앞 기호 (>> 뒤 공백)
    u8g2_DrawStr(u8g2, line4_x, 61, ">> ");
    line4_x += u8g2_GetStrWidth(u8g2, ">> ") + 1;

    // S1 상태 (타입 + 상태)
    u8g2_DrawStr(u8g2, line4_x, 61, s_switcher_data.s1_type);
    line4_x += u8g2_GetStrWidth(u8g2, s_switcher_data.s1_type) + 1;
    if (s_switcher_data.s1_connected) {
        u8g2_DrawStr(u8g2, line4_x, 61, "[V]");
    } else {
        u8g2_DrawStr(u8g2, line4_x, 61, "[X]");
    }

    // 듀얼 모드일 때만 "/"와 S2 상태 표시
    if (s_switcher_data.dual_mode) {
        line4_x += u8g2_GetStrWidth(u8g2, "[V]") + 2;
        u8g2_DrawStr(u8g2, line4_x, 61, "/");

        line4_x += u8g2_GetStrWidth(u8g2, "/") + 2;
        u8g2_DrawStr(u8g2, line4_x, 61, s_switcher_data.s2_type);
        line4_x += u8g2_GetStrWidth(u8g2, s_switcher_data.s2_type) + 1;
        if (s_switcher_data.s2_connected) {
            u8g2_DrawStr(u8g2, line4_x, 61, "[V]");
        } else {
            u8g2_DrawStr(u8g2, line4_x, 61, "[X]");
        }
    }
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
    int s1_ip_x = 2 + u8g2_GetStrWidth(u8g2, "S1 IP:") + 2;  // 라벨 너비 + 간격
    if (strlen(s_switcher_data.s1_ip) > 0) {
        u8g2_DrawStr(u8g2, s1_ip_x, 39, s_switcher_data.s1_ip);
    } else {
        u8g2_DrawStr(u8g2, s1_ip_x, 39, "---");
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
        int s2_ip_x = 2 + u8g2_GetStrWidth(u8g2, "S2 IP:") + 2;  // 라벨 너비 + 간격
        if (strlen(s_switcher_data.s2_ip) > 0) {
            u8g2_DrawStr(u8g2, s2_ip_x, 61, s_switcher_data.s2_ip);
        } else {
            u8g2_DrawStr(u8g2, s2_ip_x, 61, "---");
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
    if (s_ap_data.ap_status == TX_AP_STATUS_INACTIVE) {
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
    if (s_wifi_data.wifi_status == TX_NET_STATUS_CONNECTED && strlen(s_wifi_data.wifi_ip) > 0) {
        u8g2_DrawStr(u8g2, 25, 39, s_wifi_data.wifi_ip);
    } else {
        u8g2_DrawStr(u8g2, 25, 39, "---");
    }

    // 상태 표시
    if (s_wifi_data.wifi_status == TX_NET_STATUS_CONNECTED) {
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
    if (s_eth_data.eth_status == TX_NET_STATUS_CONNECTED && strlen(s_eth_data.eth_ip) > 0) {
        u8g2_DrawStr(u8g2, 25, 28, s_eth_data.eth_ip);
    } else {
        u8g2_DrawStr(u8g2, 25, 28, "---");
    }

    // 모드 표시
    u8g2_DrawStr(u8g2, 2, 39, s_eth_data.eth_dhcp_mode ? "DHCP" : "STATIC");

    // 상태 표시
    if (s_eth_data.eth_status == TX_NET_STATUS_CONNECTED) {
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
    u8g2_DrawStr(u8g2, 55, 28, freq_str);

    // SYNC
    u8g2_DrawStr(u8g2, 2, 39, "SYNC:");
    char sync_str[8];
    snprintf(sync_str, sizeof(sync_str), "0x%02X", s_system_data.sync_word);
    u8g2_DrawStr(u8g2, 55, 39, sync_str);

    // VOLTAGE
    u8g2_DrawStr(u8g2, 2, 50, "VOLTAGE:");
    char volt_str[10];
    snprintf(volt_str, sizeof(volt_str), "%.2f V", s_system_data.voltage);
    u8g2_DrawStr(u8g2, 55, 50, volt_str);

    // TEMP
    u8g2_DrawStr(u8g2, 2, 61, "TEMP:");
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f C", s_system_data.temperature);
    u8g2_DrawStr(u8g2, 55, 61, temp_str);

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
    // bool → enum 변환 (호환성 유지)
    s_ap_data.ap_status = enabled ? TX_AP_STATUS_ACTIVE : TX_AP_STATUS_INACTIVE;
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
    // bool → enum 변환 (호환성 유지)
    s_wifi_data.wifi_status = connected ?
        TX_NET_STATUS_CONNECTED : TX_NET_STATUS_DISCONNECTED;
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
    // bool → enum 변환 (호환성 유지)
    s_eth_data.eth_status = connected ?
        TX_NET_STATUS_CONNECTED : TX_NET_STATUS_DISCONNECTED;
}

/**
 * @brief WiFi 3단계 상태 설정 (신규 API)
 * @param status 네트워크 상태 (NOT_DETECTED/DISCONNECTED/CONNECTED)
 */
extern "C" void tx_page_set_wifi_status(tx_network_status_t status)
{
    s_wifi_data.wifi_status = status;
}

/**
 * @brief Ethernet 3단계 상태 설정 (신규 API)
 * @param status 네트워크 상태 (NOT_DETECTED/DISCONNECTED/CONNECTED)
 */
extern "C" void tx_page_set_eth_status(tx_network_status_t status)
{
    s_eth_data.eth_status = status;
}

/**
 * @brief AP 3단계 상태 설정 (신규 API)
 * @param status AP 상태 (INACTIVE/ACTIVE)
 */
extern "C" void tx_page_set_ap_status(tx_ap_status_t status)
{
    s_ap_data.ap_status = status;
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
    if (page >= 1 && page <= TX_PAGE_COUNT) {
        s_current_page = page;
    }
}

extern "C" uint8_t tx_page_get_current_page(void)
{
    return s_current_page;
}

extern "C" uint8_t tx_page_get_page_count(void)
{
    return TX_PAGE_COUNT;
}
