/**
 * @file RxPage.c
 * @brief TALLY-NODE RX Mode Page Implementation
 *
 * RX 모드에서 표시되는 페이지 구현
 * - RX1, RX2 채널 표시
 * - 중앙 정렬된 레이아웃
 */

#include "pages/RxPage.h"
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

// ConfigCore include
#include "ConfigCore.h"

// SystemMonitor include
#include "SystemMonitor.h"

// LoRaManager include
#include "LoRaManager.h"

// 상태 변수
static bool s_initialized = false;
static bool s_visible = false;

// 페이지 상태
static uint8_t s_current_page = 1;  // 1: Tally, 2: System Info

// RX 상태
static bool s_rx1_active = false;
static bool s_rx2_active = false;

// 내부 함수 선언
static void drawTallyPage(u8g2_t* u8g2);
static void drawSystemInfoPage(u8g2_t* u8g2);


// RxPage가 관리하는 함수들
esp_err_t RxPage_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // DisplayManager에서 U8g2 인스턴스를 사용하므로 여기서는 초기화하지 않음

    s_initialized = true;
    s_visible = false;

    return ESP_OK;
}

void RxPage_showPage(void)
{
    if (!s_initialized) return;

    s_visible = true;

    // U8g2 인스턴스 가져오기
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 화면 지우기
    u8g2_ClearBuffer(u8g2);

    if (s_current_page == 1) {
        // Page 1: Tally 정보
        drawTallyPage(u8g2);
    } else {
        // Page 2: 시스템 정보
        drawSystemInfoPage(u8g2);
    }

    // 버퍼 전송
    u8g2_SendBuffer(u8g2);
}

void RxPage_hidePage(void)
{
    if (!s_initialized) return;

    s_visible = false;

    DisplayHelper_clearBuffer();
    DisplayHelper_sendBuffer();
}

void RxPage_setRx1(bool active)
{
    if (!s_initialized || !s_visible) return;

    s_rx1_active = active;

    // 화면 업데이트 (sendBuffer는 호출 측에서 한 번만 실행)
    DisplayHelper_clearBuffer();
    RxPage_showPage();
}

void RxPage_setRx2(bool active)
{
    if (!s_initialized || !s_visible) return;

    s_rx2_active = active;

    // 화면 업데이트 (sendBuffer는 호출 측에서 한 번만 실행)
    DisplayHelper_clearBuffer();
    RxPage_showPage();
}


// 페이지 전환 및 관리 함수
void RxPage_switchPage(uint8_t page)
{
    if (page < 1 || page > 2) return;

    s_current_page = page;

    if (s_visible) {
        RxPage_showPage();
    }
}

uint8_t RxPage_getCurrentPage(void)
{
    return s_current_page;
}

// 즉각 업데이트를 위한 강제 업데이트 함수
void RxPage_forceUpdate(void)
{
    if (!s_initialized || !s_visible) return;

    // RxPage_showPage가 이미 ClearBuffer와 SendBuffer를 처리하므로
    // 바로 showPage 호출
    RxPage_showPage();
}

// Tally 페이지 그리기 (Page 1)
static void drawTallyPage(u8g2_t* u8g2)
{
    // DisplayManager를 통해 실제 시스템 정보 가져오기
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();
    uint8_t battery_level = getBatteryLevel(sys_info.battery_percent);

    // DisplayManager에서 가져온 실제 LoRa 정보 사용
    lora_status_t lora_status = LoRaManager::getStatus();

    // 우상단 아이콘들 (CAM 1과 라인 맞춤, Y=10)
    // 배터리 아이콘 (가장 우측)
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);

    // LoRa 신호 아이콘 (배터리 왼쪽)
    drawTallySignalIcon(u8g2, 85, 2, (int16_t)lora_status.rssi, lora_status.snr);

    // 헤더 영역 (맨 위)
#ifdef DEVICE_MODE_RX
    uint8_t camera_id = ConfigCore::getCameraId();
#else
    uint8_t camera_id = 1;  // TX 모드에서는 기본값 1
#endif

    // "CAM 1" 형태로 한 줄에 표시 (profont11 폰트)
    char cam_id_str[16];
    snprintf(cam_id_str, sizeof(cam_id_str), "CAM %d", camera_id);
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, cam_id_str);

    // 구분선 그리기
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // 화면 절반 나누기 (Y=14부터 64까지를 두 영역으로)
    // 중간 구분선
    u8g2_DrawHLine(u8g2, 0, 39, 128);

    // PGM 영역 (위쪽 절반: Y=14-39)
    // PGM 라벨 (우상단)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 25, "PGM");

    if (sys_info.tally_data_valid && sys_info.pgm_count > 0) {
        // PGM 채널 리스트 (왼쪽 정렬)
        // PGM 라벨 위치 계산 (PGM 텍스트 너비 + 여백)
        int pgm_label_width = u8g2_GetStrWidth(u8g2, "PGM");
        int max_x_pos = 110 - pgm_label_width - 5;  // PGM 라벨 왼쪽에 5픽셀 여백

        // 첫 번째 숫자는 바로 그리기
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d", sys_info.pgm_list[0]);
        u8g2_DrawStr(u8g2, 2, 34, num_str);

        // 이후 숫자들은 작은 콤마와 함께 그리기
        int x_pos = 2;
        x_pos += u8g2_GetStrWidth(u8g2, num_str);

        bool overflow = false;  // 넘침 여부 확인

        // 최대 6개까지만 표시
        for (uint8_t i = 1; i < sys_info.pgm_count && i < 6; i++) {
            // 작은 콤마와 다음 숫자의 너비 계산
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            int comma_width = u8g2_GetStrWidth(u8g2, ",");

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            char next_num_str[8];
            snprintf(next_num_str, sizeof(next_num_str), "%d", sys_info.pgm_list[i]);
            int next_num_width = u8g2_GetStrWidth(u8g2, next_num_str);

            // 다음 요소(콤마+숫자)가 최대 위치를 넘어가는지 확인
            if (x_pos + comma_width + next_num_width > max_x_pos) {
                overflow = true;
                break;
            }

            // 작은 콤마 그리기
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            u8g2_DrawStr(u8g2, x_pos, 34, ",");
            x_pos += comma_width;

            // 큰 숫자 그리기
            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            u8g2_DrawStr(u8g2, x_pos, 34, next_num_str);
            x_pos += next_num_width;
        }

        // 넘쳤을 경우 "..." 표시
        if (overflow) {
            // 작은 폰트로 ... 표시
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            // 마지막 숫자를 지우고 ...으로 대체
            // 마지막 숫자의 너비를 구하고 그 위치에 ...을 그림
            if (x_pos > 10) {  // 최소한의 공간이 있을 때만
                x_pos -= u8g2_GetStrWidth(u8g2, num_str);  // 마지막 숫자 너비만큼 뒤로
                u8g2_DrawStr(u8g2, x_pos + 8, 34, "...");
            }
        }
    } else {
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        u8g2_DrawStr(u8g2, 2, 34, "---");
    }

    // PVW 영역 (아래쪽 절반: Y=39-64)
    // PVW 라벨 (우상단)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 50, "PVW");

    if (sys_info.tally_data_valid && sys_info.pvw_count > 0) {
        // PVW 채널 리스트 (왼쪽 정렬)
        // PVW 라벨 위치 계산 (PVW 텍스트 너비 + 여백)
        int pvw_label_width = u8g2_GetStrWidth(u8g2, "PVW");
        int max_x_pos = 110 - pvw_label_width - 5;  // PVW 라벨 왼쪽에 5픽셀 여백

        // 첫 번째 숫자는 바로 그리기
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d", sys_info.pvw_list[0]);
        u8g2_DrawStr(u8g2, 2, 59, num_str);

        // 이후 숫자들은 작은 콤마와 함께 그리기
        int x_pos = 2;
        x_pos += u8g2_GetStrWidth(u8g2, num_str);

        bool overflow = false;  // 넘침 여부 확인

        // 최대 6개까지만 표시
        for (uint8_t i = 1; i < sys_info.pvw_count && i < 6; i++) {
            // 작은 콤마와 다음 숫자의 너비 계산
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            int comma_width = u8g2_GetStrWidth(u8g2, ",");

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            char next_num_str[8];
            snprintf(next_num_str, sizeof(next_num_str), "%d", sys_info.pvw_list[i]);
            int next_num_width = u8g2_GetStrWidth(u8g2, next_num_str);

            // 다음 요소(콤마+숫자)가 최대 위치를 넘어가는지 확인
            if (x_pos + comma_width + next_num_width > max_x_pos) {
                overflow = true;
                break;
            }

            // 작은 콤마 그리기
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            u8g2_DrawStr(u8g2, x_pos, 59, ",");
            x_pos += comma_width;

            // 큰 숫자 그리기
            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            u8g2_DrawStr(u8g2, x_pos, 59, next_num_str);
            x_pos += next_num_width;
        }

        // 넘쳤을 경우 "..." 표시
        if (overflow) {
            // 작은 폰트로 ... 표시
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            // 마지막 숫자를 지우고 ...으로 대체
            // 마지막 숫자의 너비를 구하고 그 위치에 ...을 그림
            if (x_pos > 10) {  // 최소한의 공간이 있을 때만
                x_pos -= u8g2_GetStrWidth(u8g2, num_str);  // 마지막 숫자 너비만큼 뒤로
                u8g2_DrawStr(u8g2, x_pos + 8, 59, "...");
            }
        }
    } else {
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        u8g2_DrawStr(u8g2, 2, 59, "---");
    }
}

// 시스템 정보 페이지 그리기 (Page 2) - 3x2 테이블 형식 (6개 항목)
static void drawSystemInfoPage(u8g2_t* u8g2)
{
    // DisplayManager를 통해 실제 시스템 정보 가져오기
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();
    uint8_t battery_level = getBatteryLevel(sys_info.battery_percent);

    // DisplayManager에서 가져온 실제 LoRa 정보 사용
    lora_status_t lora_status = LoRaManager::getStatus();

    // 우상단 아이콘들 (SYSTEM과 라인 맞춤, Y=10)
    // 배터리 아이콘 (가장 우측)
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);

    // LoRa 신호 아이콘 (배터리 왼쪽)
    drawTallySignalIcon(u8g2, 85, 2, (int16_t)lora_status.rssi, lora_status.snr);

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
    strncpy(id_short, s_system_info.device_id, 8);
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