/**
 * @file TxPage.cpp
 * @brief TALLY-NODE TX Mode Page Implementation
 *
 * TX 모드에서 표시되는 페이지 구현
 * - Page 1: 스위처 연결 정보
 * - Page 2: 네트워크 설정 정보
 * - Page 3: 시스템 정보
 */

// TX 모드에서만 컴파일되도록 조건 추가
#ifdef DEVICE_MODE_TX

#include "pages/TxPage.h"
#include "core/DisplayHelper.h"
#include "core/DisplayManager.h"
#include "u8g2.h"
#include "ui/icons.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

// PageManager include (모든 정보는 PageManager를 통해 가져옴)
#include "PageManager.h"

// SystemMonitor include
#include "SystemMonitor.h"

// LoRaManager include
#include "LoRaManager.h"


// 상태 변수
static bool s_initialized = false;
static bool s_visible = false;

// 페이지 상태
static uint8_t s_current_page = 1;  // 1: Switcher, 2: Network, 3: System

// 페이지 버퍼 구조체 (변경 감지용)
typedef struct {
    bool dual_mode;
    char s1_type[16];
    char s1_ip[32];
    uint16_t s1_port;
    bool s1_connected;  // S1 연결 상태
    char s2_type[16];
    char s2_ip[32];
    uint16_t s2_port;
    bool s2_connected;  // S2 연결 상태
    bool valid;  // 버퍼 유효성 플래그
} page1_buffer_t;

static page1_buffer_t s_page1_buffer = {
    .dual_mode = false,
    .s1_type = {0},
    .s1_ip = {0},
    .s1_port = 0,
    .s1_connected = false,
    .s2_type = {0},
    .s2_ip = {0},
    .s2_port = 0,
    .s2_connected = false,
    .valid = false
};

// 내부 함수 선언
static void drawSwitcherPage(u8g2_t* u8g2);
static void drawNetworkPage(u8g2_t* u8g2);
static void drawSystemPage(u8g2_t* u8g2);
static bool hasPage1Changed(void);

// TxPage가 관리하는 함수들
esp_err_t TxPage_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_initialized = true;
    s_visible = false;

    // 페이지를 1로 초기화
    s_current_page = 1;
    s_page1_buffer.valid = false;  // 버퍼 초기화

    return ESP_OK;
}

void TxPage_showPage(void)
{
    if (!s_initialized) return;

    s_visible = true;

    // U8g2 인스턴스 가져오기
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // Page 1은 변경이 있을 때만 업데이트
    if (s_current_page == 1) {
        if (!s_page1_buffer.valid || hasPage1Changed()) {
            // 화면 지우기
            u8g2_ClearBuffer(u8g2);

            // Page 1: 스위처 정보
            drawSwitcherPage(u8g2);

            // 버퍼 전송
            u8g2_SendBuffer(u8g2);

            // 버퍼 업데이트
            s_page1_buffer.valid = true;
        }
    } else {
        // Page 2, 3은 항상 업데이트
        // 화면 지우기
        u8g2_ClearBuffer(u8g2);

        if (s_current_page == 2) {
            // Page 2: 네트워크 정보
            drawNetworkPage(u8g2);
        } else {
            // Page 3: 시스템 정보
            drawSystemPage(u8g2);
        }

        // 버퍼 전송
        u8g2_SendBuffer(u8g2);
    }
}

void TxPage_hidePage(void)
{
    if (!s_initialized) return;

    s_visible = false;

    DisplayHelper_clearBuffer();
    DisplayHelper_sendBuffer();
}

// 페이지 전환 및 관리 함수
void TxPage_switchPage(uint8_t page)
{
    if (page < 1 || page > 3) return;

    // 페이지가 변경되면 버퍼 무효화 (항상 업데이트하도록)
    if (s_current_page != page) {
        s_page1_buffer.valid = false;
    }

    s_current_page = page;

    if (s_visible) {
        TxPage_showPage();
    }
}

uint8_t TxPage_getCurrentPage(void)
{
    return s_current_page;
}

// 즉각 업데이트를 위한 강제 업데이트 함수
void TxPage_forceUpdate(void)
{
    if (!s_initialized || !s_visible) return;

    TxPage_showPage();
}

// 스위처 정보 페이지 그리기 (Page 1)
static void drawSwitcherPage(u8g2_t* u8g2)
{
    // DisplayManager를 통해 실제 시스템 정보 가져오기
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();
    uint8_t battery_level = getBatteryLevel(sys_info.battery_percent);

    // 우상단 배터리 아이콘만 표시
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);

    // PageManager를 통해 모든 정보 가져오기
    bool dual_mode = PageManager_getDualMode();
    bool s1_connected = PageManager_isSwitcherConnected(PAGE_SWITCHER_PRIMARY);
    bool s2_connected = PageManager_isSwitcherConnected(PAGE_SWITCHER_SECONDARY);

    // 버퍼 업데이트
    s_page1_buffer.dual_mode = dual_mode;

    // S1 정보 가져오기
    const char* s1_type_str = PageManager_getSwitcherType(PAGE_SWITCHER_PRIMARY);
    const char* s1_ip = PageManager_getSwitcherIp(PAGE_SWITCHER_PRIMARY);
    uint16_t s1_port = PageManager_getSwitcherPort(PAGE_SWITCHER_PRIMARY);

    // 버퍼 업데이트 (NULL 체크 추가)
    if (s1_type_str) {
        strncpy(s_page1_buffer.s1_type, s1_type_str, sizeof(s_page1_buffer.s1_type) - 1);
        s_page1_buffer.s1_type[sizeof(s_page1_buffer.s1_type) - 1] = '\0';
    } else {
        s_page1_buffer.s1_type[0] = '\0';
    }

    if (s1_ip) {
        strncpy(s_page1_buffer.s1_ip, s1_ip, sizeof(s_page1_buffer.s1_ip) - 1);
        s_page1_buffer.s1_ip[sizeof(s_page1_buffer.s1_ip) - 1] = '\0';
    } else {
        s_page1_buffer.s1_ip[0] = '\0';
    }
    s_page1_buffer.s1_port = s1_port;

    // S2 정보 가져오기
    const char* s2_type_str = PageManager_getSwitcherType(PAGE_SWITCHER_SECONDARY);
    const char* s2_ip = PageManager_getSwitcherIp(PAGE_SWITCHER_SECONDARY);
    uint16_t s2_port = PageManager_getSwitcherPort(PAGE_SWITCHER_SECONDARY);

    // 버퍼 업데이트 (NULL 체크 추가)
    if (s2_type_str) {
        strncpy(s_page1_buffer.s2_type, s2_type_str, sizeof(s_page1_buffer.s2_type) - 1);
        s_page1_buffer.s2_type[sizeof(s_page1_buffer.s2_type) - 1] = '\0';
    } else {
        s_page1_buffer.s2_type[0] = '\0';
    }

    if (s2_ip) {
        strncpy(s_page1_buffer.s2_ip, s2_ip, sizeof(s_page1_buffer.s2_ip) - 1);
        s_page1_buffer.s2_ip[sizeof(s_page1_buffer.s2_ip) - 1] = '\0';
    } else {
        s_page1_buffer.s2_ip[0] = '\0';
    }
    s_page1_buffer.s2_port = s2_port;

    // 연결 상태도 버퍼에 저장
    s_page1_buffer.s1_connected = s1_connected;
    s_page1_buffer.s2_connected = s2_connected;

    // 헤더에 모드 표시 (SINGLE/DUAL)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "Mode:");
    const char* mode_str = dual_mode ? "DUAL" : "SINGLE";
    u8g2_DrawStr(u8g2, 40, 10, mode_str);

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);
    
    // S1 정보 (항상 표시)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 28, "S1:");

    // "S1: ATEM [OK]/[FAIL]" 형태로 왼쪽 정렬
    int s1_x = 25;  // "S1:" 다음 위치
    const char* type_to_draw = s1_type_str ? s1_type_str : "NONE";
    u8g2_DrawStr(u8g2, s1_x, 28, type_to_draw);
    int s1_type_end = s1_x + u8g2_GetStrWidth(u8g2, type_to_draw) + 5;

    // 실제 연결 상태 확인 (s1_connected은 이미 상단에서 선언됨)
    const char* s1_status = s1_connected ? "[OK]" : "[FAIL]";
    u8g2_DrawStr(u8g2, s1_type_end, 28, s1_status);

    // 두 번째 줄: IP 주소
    u8g2_DrawStr(u8g2, 2, 39, "IP:");
    if (s1_ip && strlen(s1_ip) > 0) {
        u8g2_DrawStr(u8g2, 25, 39, s1_ip);
    }

    // DUAL 모드일 때만 S2 정보 표시 (S1 아래쪽)
    if (dual_mode) {
        // 아래쪽 영역: S2 정보
        u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
        u8g2_DrawStr(u8g2, 2, 51, "S2:");

        // "S2: ATEM [OK]/[FAIL]" 형태로 왼쪽 정렬
        int s2_x = 25;  // "S2:" 다음 위치
        const char* s2_type_to_draw = s2_type_str ? s2_type_str : "NONE";
        u8g2_DrawStr(u8g2, s2_x, 51, s2_type_to_draw);
        int s2_type_end = s2_x + u8g2_GetStrWidth(u8g2, s2_type_to_draw) + 5;

        // 실제 연결 상태 확인 (s2_connected은 이미 상단에서 선언됨)
        const char* s2_status = s2_connected ? "[OK]" : "[FAIL]";
        u8g2_DrawStr(u8g2, s2_type_end, 51, s2_status);

        // 두 번째 줄: IP 주소
        u8g2_DrawStr(u8g2, 2, 62, "IP:");
        if (s2_ip && strlen(s2_ip) > 0) {
            u8g2_DrawStr(u8g2, 25, 62, s2_ip);
        }
    }
}

// 네트워크 정보 페이지 그리기 (Page 2)
static void drawNetworkPage(u8g2_t* u8g2)
{
    // DisplayManager를 통해 실제 시스템 정보 가져오기
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();
    uint8_t battery_level = getBatteryLevel(sys_info.battery_percent);

    // 우상단 배터리 아이콘만 표시
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "NETWORK");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // 네트워크 정보 표시
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // WiFi AP (항상 활성화)
    u8g2_DrawStr(u8g2, 2, 28, "AP:");
    u8g2_DrawStr(u8g2, 30, 28, sys_info.wifi_ap_ip);

    // WiFi STA
    u8g2_DrawStr(u8g2, 2, 40, "STA:");
    if (sys_info.wifi_sta_connected && strlen(sys_info.wifi_sta_ip) > 0) {
        u8g2_DrawStr(u8g2, 30, 40, sys_info.wifi_sta_ip);
    } else {
        u8g2_DrawStr(u8g2, 30, 40, "---");
    }

    // Ethernet
    u8g2_DrawStr(u8g2, 2, 52, "ETH:");
    if (sys_info.eth_link_up && strlen(sys_info.eth_ip) > 0) {
        u8g2_DrawStr(u8g2, 30, 52, sys_info.eth_ip);
    } else {
        u8g2_DrawStr(u8g2, 30, 52, "---");
    }
}

// 시스템 정보 페이지 그리기 (Page 3) - RX Page 2와 동일
static void drawSystemPage(u8g2_t* u8g2)
{
    // DisplayManager를 통해 실제 시스템 정보 가져오기
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();
    uint8_t battery_level = getBatteryLevel(sys_info.battery_percent);

    // DisplayManager에서 가져온 실제 LoRa 정보 사용
    lora_status_t lora_status = LoRaManager::getStatus();

    // 우상단 배터리 아이콘만 표시
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "SYSTEM");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // SystemMonitor 및 InfoManager에서 정보 가져오기
    SystemHealth health = SystemMonitor::getHealth();

    // 테이블 그리기 (3x2)
    // 세로선 1개 (2열)
    u8g2_DrawVLine(u8g2, 64, 16, 48);   // 세로선

    // 가로선 2개 (3행)
    u8g2_DrawHLine(u8g2, 0, 32, 128);   // 첫 번째 가로선
    u8g2_DrawHLine(u8g2, 0, 48, 128);   // 두 번째 가로선

    // 모든 텍스트는 profont11_mf 사용
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // 1행 1열: Device ID (가운데 정렬)
    char id_short[10];
    strncpy(id_short, sys_info.device_id, 8);
    id_short[8] = '\0';
    int id_width = u8g2_GetStrWidth(u8g2, id_short);
    u8g2_DrawStr(u8g2, 32 - (id_width / 2), 27, id_short);

    // 1행 2열: Uptime (가운데 정렬, hh:mm:ss 형식)
    char uptime_str[12];
    uint64_t uptime_sec = health.uptime_sec;
    uint32_t hours = (uptime_sec / 3600) % 100;  // 최대 99시간
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    snprintf(uptime_str, sizeof(uptime_str), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    int uptime_width = u8g2_GetStrWidth(u8g2, uptime_str);
    u8g2_DrawStr(u8g2, 98 - (uptime_width / 2), 27, uptime_str);

    // 2행 1열: Frequency (가운데 정렬)
    char freq_str[10];
    snprintf(freq_str, sizeof(freq_str), "%.0fMHz", lora_status.frequency);
    int freq_width = u8g2_GetStrWidth(u8g2, freq_str);
    u8g2_DrawStr(u8g2, 32 - (freq_width / 2), 44, freq_str);

    // 2행 2열: Sync Word (가운데 정렬)
    char sync_str[8];
    snprintf(sync_str, sizeof(sync_str), "0x%02X", lora_status.sync_word);
    int sync_width = u8g2_GetStrWidth(u8g2, sync_str);
    u8g2_DrawStr(u8g2, 98 - (sync_width / 2), 44, sync_str);

    // 3행 1열: Voltage (가운데 정렬, 소수점 2자리)
    char volt_str[8];
    snprintf(volt_str, sizeof(volt_str), "%.2fV", health.voltage);
    int volt_width = u8g2_GetStrWidth(u8g2, volt_str);
    u8g2_DrawStr(u8g2, 32 - (volt_width / 2), 60, volt_str);

    // 3행 2열: Temperature (가운데 정렬, 소수점 1자리)
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1fC", health.temperature_celsius);
    int temp_width = u8g2_GetStrWidth(u8g2, temp_str);
    u8g2_DrawStr(u8g2, 98 - (temp_width / 2), 60, temp_str);
}

// 페이지 1 변경 감지 함수
static bool hasPage1Changed(void)
{
    if (!s_page1_buffer.valid) {
        return true;  // 버퍼가 유효하지 않으면 변경된 것으로 간주
    }

    // PageManager를 통해 현재 값 가져오기
    bool current_dual_mode = PageManager_getDualMode();
    const char* current_s1_type = PageManager_getSwitcherType(PAGE_SWITCHER_PRIMARY);
    const char* current_s1_ip = PageManager_getSwitcherIp(PAGE_SWITCHER_PRIMARY);
    uint16_t current_s1_port = PageManager_getSwitcherPort(PAGE_SWITCHER_PRIMARY);
    bool current_s1_connected = PageManager_isSwitcherConnected(PAGE_SWITCHER_PRIMARY);

    const char* current_s2_type = PageManager_getSwitcherType(PAGE_SWITCHER_SECONDARY);
    const char* current_s2_ip = PageManager_getSwitcherIp(PAGE_SWITCHER_SECONDARY);
    uint16_t current_s2_port = PageManager_getSwitcherPort(PAGE_SWITCHER_SECONDARY);
    bool current_s2_connected = PageManager_isSwitcherConnected(PAGE_SWITCHER_SECONDARY);

    // 변경 여부 확인
    bool changed = false;

    if (s_page1_buffer.dual_mode != current_dual_mode) {
        changed = true;
    }
    if (!current_s1_type || strcmp(s_page1_buffer.s1_type, current_s1_type) != 0) {
        changed = true;
    }
    if (!current_s1_ip || strcmp(s_page1_buffer.s1_ip, current_s1_ip) != 0) {
        changed = true;
    }
    if (s_page1_buffer.s1_port != current_s1_port) {
        changed = true;
    }
    if (s_page1_buffer.s1_connected != current_s1_connected) {
        changed = true;
    }
    if (!current_s2_type || strcmp(s_page1_buffer.s2_type, current_s2_type) != 0) {
        changed = true;
    }
    if (!current_s2_ip || strcmp(s_page1_buffer.s2_ip, current_s2_ip) != 0) {
        changed = true;
    }
    if (s_page1_buffer.s2_port != current_s2_port) {
        changed = true;
    }
    if (s_page1_buffer.s2_connected != current_s2_connected) {
        changed = true;
    }

    return changed;
}

#endif // DEVICE_MODE_TX

