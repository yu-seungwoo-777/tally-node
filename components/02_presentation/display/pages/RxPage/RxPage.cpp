/**
 * @file RxPage.cpp
 * @brief RX 모드 페이지 구현 (LoRa 수신 Tally 상태)
 *
 * 2개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 시스템 정보
 */

#include "RxPage.h"
#include "icons.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "RxPage";

// ============================================================================
// 내부 상태
// ============================================================================

// Tally 데이터
static struct {
    uint8_t pgm_channels[20];
    uint8_t pgm_count;
    uint8_t pvw_channels[20];
    uint8_t pvw_count;
    uint8_t cam_id;
} s_tally_data = {
    .pgm_channels = {0},
    .pgm_count = 0,
    .pvw_channels = {0},
    .pvw_count = 0,
    .cam_id = 1,
};

// 시스템 정보 데이터
static struct {
    uint8_t battery_percent;
    int16_t rssi;
    float snr;
    float frequency;
    uint8_t sync_word;
    float voltage;
    float temperature;
    char device_id[17];
    uint64_t uptime_sec;
} s_system_data = {
    .battery_percent = 75,
    .rssi = -120,
    .snr = 0.0f,
    .frequency = 868.0f,
    .sync_word = 0x12,
    .voltage = 3.7f,
    .temperature = 25.0f,
    .device_id = "????????",
    .uptime_sec = 0,
};

// 현재 페이지 (1: Tally, 2: System)
static uint8_t s_current_page = 1;

// 페이지 상태 (NORMAL, CAMERA_ID)
static rx_page_state_t s_page_state = RX_PAGE_STATE_NORMAL;

// 카메라 ID 팝업 관련 변수
static uint8_t s_display_camera_id = 1;    // 팝업에 표시될 ID
static bool s_camera_id_changing = false;  // ID 자동 변경 중

// ============================================================================
// 내부 함수 선언
// ============================================================================

static void draw_rx_header(u8g2_t* u8g2);
static void draw_tally_page(u8g2_t* u8g2);
static void draw_system_page(u8g2_t* u8g2);
static void draw_camera_id_popup(u8g2_t* u8g2);

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "RxPage 초기화");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    // 페이지 상태에 따라 다르게 렌더링
    if (s_page_state == RX_PAGE_STATE_CAMERA_ID) {
        draw_camera_id_popup(u8g2);
        return;
    }

    // 일반 상태: 현재 페이지에 따라 렌더링
    if (s_current_page == 1) {
        draw_tally_page(u8g2);
    } else {
        draw_system_page(u8g2);
    }
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "RxPage 진입 (page %d)", s_current_page);
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "RxPage 퇴장");
}

// ============================================================================
// 페이지 인터페이스 정의
// ============================================================================

static const display_page_interface_t s_rx_page_interface = {
    .id = PAGE_RX,
    .name = "RX",
    .init = page_init,
    .render = page_render,
    .on_enter = page_on_enter,
    .on_exit = page_on_exit,
};

// ============================================================================
// 내부 함수 구현
// ============================================================================

/**
 * @brief RX 헤더 그리기 (배터리 + 신호 아이콘)
 */
static void draw_rx_header(u8g2_t* u8g2)
{
    uint8_t battery_level = getBatteryLevel(s_system_data.battery_percent);
    drawTallyBatteryIcon(u8g2, 105, 2, battery_level);
    T_LOGD(TAG, "draw_rx_header: RSSI=%d SNR=%.1f", s_system_data.rssi, s_system_data.snr);
    drawTallySignalIcon(u8g2, 85, 2, s_system_data.rssi, s_system_data.snr);
}

/**
 * @brief Tally 페이지 그리기 (Page 1)
 */
static void draw_tally_page(u8g2_t* u8g2)
{
    draw_rx_header(u8g2);

    // 헤더: CAM ID
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char cam_str[16];
    snprintf(cam_str, sizeof(cam_str), "CAM %d", s_tally_data.cam_id);
    u8g2_DrawStr(u8g2, 2, 10, cam_str);

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    // 화면 절반 나누기
    u8g2_DrawHLine(u8g2, 0, 39, 128);

    // PGM 영역 (위쪽 절반)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 26, "PGM");

    if (s_tally_data.pgm_count > 0) {
        int max_x_pos = 110 - u8g2_GetStrWidth(u8g2, "PGM") - 5;

        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d", s_tally_data.pgm_channels[0]);
        u8g2_DrawStr(u8g2, 2, 34, num_str);

        int x_pos = 2 + u8g2_GetStrWidth(u8g2, num_str);
        bool overflow = false;

        for (uint8_t i = 1; i < s_tally_data.pgm_count && i < 6; i++) {
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            int comma_width = u8g2_GetStrWidth(u8g2, ",");

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            char next_num_str[8];
            snprintf(next_num_str, sizeof(next_num_str), "%d", s_tally_data.pgm_channels[i]);
            int next_num_width = u8g2_GetStrWidth(u8g2, next_num_str);

            if (x_pos + comma_width + next_num_width > max_x_pos) {
                overflow = true;
                break;
            }

            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            u8g2_DrawStr(u8g2, x_pos, 34, ",");
            x_pos += comma_width;

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            u8g2_DrawStr(u8g2, x_pos, 34, next_num_str);
            x_pos += next_num_width;
        }

        if (overflow) {
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            if (x_pos > 10) {
                x_pos -= u8g2_GetStrWidth(u8g2, num_str);
                u8g2_DrawStr(u8g2, x_pos + 8, 34, "...");
            }
        }
    } else {
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        u8g2_DrawStr(u8g2, 2, 34, "---");
    }

    // PVW 영역 (아래쪽 절반)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 110, 51, "PVW");

    if (s_tally_data.pvw_count > 0) {
        int max_x_pos = 110 - u8g2_GetStrWidth(u8g2, "PVW") - 5;

        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        char num_str[8];
        snprintf(num_str, sizeof(num_str), "%d", s_tally_data.pvw_channels[0]);
        u8g2_DrawStr(u8g2, 2, 59, num_str);

        int x_pos = 2 + u8g2_GetStrWidth(u8g2, num_str);
        bool overflow = false;

        for (uint8_t i = 1; i < s_tally_data.pvw_count && i < 6; i++) {
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            int comma_width = u8g2_GetStrWidth(u8g2, ",");

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            char next_num_str[8];
            snprintf(next_num_str, sizeof(next_num_str), "%d", s_tally_data.pvw_channels[i]);
            int next_num_width = u8g2_GetStrWidth(u8g2, next_num_str);

            if (x_pos + comma_width + next_num_width > max_x_pos) {
                overflow = true;
                break;
            }

            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            u8g2_DrawStr(u8g2, x_pos, 59, ",");
            x_pos += comma_width;

            u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
            u8g2_DrawStr(u8g2, x_pos, 59, next_num_str);
            x_pos += next_num_width;
        }

        if (overflow) {
            u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
            if (x_pos > 10) {
                x_pos -= u8g2_GetStrWidth(u8g2, num_str);
                u8g2_DrawStr(u8g2, x_pos + 8, 59, "...");
            }
        }
    } else {
        u8g2_SetFont(u8g2, u8g2_font_profont22_mf);
        u8g2_DrawStr(u8g2, 2, 59, "---");
    }
}

/**
 * @brief 시스템 정보 페이지 그리기 (Page 2)
 */
static void draw_system_page(u8g2_t* u8g2)
{
    draw_rx_header(u8g2);

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

extern "C" bool rx_page_init(void)
{
    return display_manager_register_page(&s_rx_page_interface);
}

// ========== Tally 데이터 설정 ==========

extern "C" void rx_page_set_pgm_channels(const uint8_t* channels, uint8_t count)
{
    s_tally_data.pgm_count = (count > 20) ? 20 : count;
    if (channels != nullptr && s_tally_data.pgm_count > 0) {
        memcpy(s_tally_data.pgm_channels, channels,
               s_tally_data.pgm_count * sizeof(uint8_t));
    }
}

extern "C" void rx_page_set_pvw_channels(const uint8_t* channels, uint8_t count)
{
    s_tally_data.pvw_count = (count > 20) ? 20 : count;
    if (channels != nullptr && s_tally_data.pvw_count > 0) {
        memcpy(s_tally_data.pvw_channels, channels,
               s_tally_data.pvw_count * sizeof(uint8_t));
    }
}

extern "C" void rx_page_set_cam_id(uint8_t cam_id)
{
    s_tally_data.cam_id = cam_id;
}

// ========== 시스템 정보 설정 ==========

extern "C" void rx_page_set_battery(uint8_t percent)
{
    s_system_data.battery_percent = (percent > 100) ? 100 : percent;
}

extern "C" void rx_page_set_rssi(int16_t rssi)
{
    s_system_data.rssi = rssi;
    T_LOGD(TAG, "RSSI 설정: %d", rssi);
}

extern "C" void rx_page_set_snr(float snr)
{
    s_system_data.snr = snr;
}

extern "C" void rx_page_set_frequency(float freq_mhz)
{
    s_system_data.frequency = freq_mhz;
}

extern "C" void rx_page_set_sync_word(uint8_t sync_word)
{
    s_system_data.sync_word = sync_word;
}

extern "C" void rx_page_set_voltage(float voltage)
{
    s_system_data.voltage = voltage;
}

extern "C" void rx_page_set_temperature(float temp)
{
    s_system_data.temperature = temp;
}

extern "C" void rx_page_set_device_id(const char* device_id)
{
    if (device_id != nullptr) {
        strncpy(s_system_data.device_id, device_id, 16);
        s_system_data.device_id[16] = '\0';
    }
}

extern "C" void rx_page_set_uptime(uint64_t uptime_sec)
{
    s_system_data.uptime_sec = uptime_sec;
}

// ========== 페이지 제어 ==========

extern "C" void rx_page_switch_page(uint8_t page)
{
    if (page >= 1 && page <= 2) {
        s_current_page = page;
    }
}

extern "C" uint8_t rx_page_get_current_page(void)
{
    return s_current_page;
}

// ========== 카메라 ID 변경 팝업 제어 ==========

extern "C" void rx_page_set_state(rx_page_state_t state)
{
    s_page_state = state;
}

extern "C" rx_page_state_t rx_page_get_state(void)
{
    return s_page_state;
}

extern "C" void rx_page_show_camera_id_popup(void)
{
    rx_page_show_camera_id_popup_with_max(20);  // 기본값
}

/**
 * @brief 카메라 ID 변경 팝업 표시 (최대값 지정)
 * @param max_camera_num 최대 카메라 번호
 */
extern "C" void rx_page_show_camera_id_popup_with_max(uint8_t max_camera_num)
{
    s_page_state = RX_PAGE_STATE_CAMERA_ID;
    // 현재 저장된 카메라 ID로 표시 ID 초기화 (max로 clamping)
    s_display_camera_id = s_tally_data.cam_id;
    if (s_display_camera_id > max_camera_num) {
        s_display_camera_id = 1;  // max를 초과하면 1로 리셋
    }
    s_camera_id_changing = false;
    T_LOGD(TAG, "Camera ID 팝업 표시 (ID: %d, max: %d)", s_display_camera_id, max_camera_num);
}

extern "C" void rx_page_hide_camera_id_popup(void)
{
    s_page_state = RX_PAGE_STATE_NORMAL;
    s_camera_id_changing = false;
    T_LOGD(TAG, "Camera ID 팝업 숨김");
}

extern "C" uint8_t rx_page_get_display_camera_id(void)
{
    return s_display_camera_id;
}

extern "C" void rx_page_set_display_camera_id(uint8_t cam_id)
{
    if (cam_id >= 1 && cam_id <= 20) {
        s_display_camera_id = cam_id;
    }
}

extern "C" void rx_page_set_camera_id_changing(bool changing)
{
    s_camera_id_changing = changing;
}

extern "C" bool rx_page_is_camera_id_changing(void)
{
    return s_camera_id_changing;
}

extern "C" uint8_t rx_page_cycle_camera_id(uint8_t max_camera_num)
{
    if (s_display_camera_id >= max_camera_num) {
        s_display_camera_id = 1;
    } else {
        s_display_camera_id++;
    }
    T_LOGD(TAG, "Camera ID 순환: %d (최대: %d)", s_display_camera_id, max_camera_num);
    return s_display_camera_id;
}

// ============================================================================
// 내부 함수 구현 (카메라 ID 팝업)
// ============================================================================

/**
 * @brief 카메라 ID 변경 팝업 그리기
 * SettingsPage.cpp의 drawCameraIdPopup() 디자인 참고
 */
static void draw_camera_id_popup(u8g2_t* u8g2)
{
    // 팝업 박스 좌표
    int popup_x = 2;
    int popup_y = 2;
    int popup_w = 124;
    int popup_h = 60;

    // 팝업 배경 (흰색)
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, popup_x, popup_y, popup_w, popup_h);

    // 팝업 테두리 (2줄 - 최외곽, 검은색)
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, popup_x, popup_y, popup_w, popup_h);
    u8g2_DrawFrame(u8g2, popup_x + 1, popup_y + 1, popup_w - 2, popup_h - 2);

    // 상단 제목 "CAMERA ID" (검은색)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    int title_width = u8g2_GetStrWidth(u8g2, "CAMERA ID");
    u8g2_DrawStr(u8g2, (128 - title_width) / 2, popup_y + 15, "CAMERA ID");

    // 구분선 (더 길게)
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 22, popup_w - 10);
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 23, popup_w - 10);

    // ID 표시 (중앙 큰 글자, 검은색)
    char id_str[8];
    snprintf(id_str, sizeof(id_str), "%d", s_display_camera_id);

    u8g2_SetFont(u8g2, u8g2_font_profont29_mn);
    int id_width = u8g2_GetStrWidth(u8g2, id_str);
    u8g2_DrawStr(u8g2, (128 - id_width) / 2, popup_y + 50, id_str);
}
