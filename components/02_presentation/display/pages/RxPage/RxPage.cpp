/**
 * @file RxPage.cpp
 * @brief RX 모드 페이지 구현 (LoRa 수신 Tally 상태)
 *
 * 3개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 시스템 정보
 * - Page 3: RX 수신 통계 (RSSI, SNR, 수신 간격, 총 수신 개수)
 */

#include "RxPage.h"
#include "icons.h"
#include "t_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "02_RxPage";

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

// RX 통계 데이터
static struct {
    int16_t last_rssi;       // 마지막 RSSI (dBm)
    float last_snr;          // 마지막 SNR (dB)
    uint32_t interval;       // 마지막 Tally 수신 간격 (ms)
} s_rx_stats = {
    .last_rssi = -120,
    .last_snr = 0.0f,
    .interval = 0,
};

// 현재 페이지 (1: Tally, 2: System, 3: RX Stats)
static uint8_t s_current_page = 1;

// 페이지 상태 (NORMAL, CAMERA_ID)
static rx_page_state_t s_page_state = RX_PAGE_STATE_NORMAL;

// 카메라 ID 팝업 관련 변수
static uint8_t s_display_camera_id = 1;    // 팝업에 표시될 ID
static bool s_camera_id_changing = false;  // ID 자동 변경 중

// 기능 정지 상태
static bool s_stopped = false;  // 기능 정지 상태 플래그

// ============================================================================
// 내부 함수 선언
// ============================================================================

static void draw_rx_header(u8g2_t* u8g2);
static void draw_channel_list(u8g2_t* u8g2, const uint8_t* channels, uint8_t count,
                              int y_pos, int max_width);
static void draw_tally_page(u8g2_t* u8g2);
static void draw_system_page(u8g2_t* u8g2);
static void draw_rx_stats_page(u8g2_t* u8g2);
static void draw_camera_id_popup(u8g2_t* u8g2);
static void draw_stopped_popup(u8g2_t* u8g2);

// ============================================================================
// 페이지 인터페이스 구현
// ============================================================================

/**
 * @brief 페이지 초기화
 */
static void page_init(void)
{
    T_LOGI(TAG, "RxPage initialized");
}

/**
 * @brief 렌더링
 */
static void page_render(u8g2_t* u8g2)
{
    // 기능 정지 상태: 정지 팝업 우선 표시
    if (s_stopped) {
        draw_stopped_popup(u8g2);
        return;
    }

    // 페이지 상태에 따라 다르게 렌더링
    if (s_page_state == RX_PAGE_STATE_CAMERA_ID) {
        draw_camera_id_popup(u8g2);
        return;
    }

    // 일반 상태: 현재 페이지에 따라 렌더링
    if (s_current_page == 1) {
        draw_tally_page(u8g2);
    } else if (s_current_page == 2) {
        draw_system_page(u8g2);
    } else {
        draw_rx_stats_page(u8g2);
    }
}

/**
 * @brief 페이지 진입
 */
static void page_on_enter(void)
{
    T_LOGD(TAG, "RxPage entered (page %d)", s_current_page);
}

/**
 * @brief 페이지 퇴장
 */
static void page_on_exit(void)
{
    T_LOGD(TAG, "RxPage exited");
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
    drawTallySignalIcon(u8g2, 85, 2, s_system_data.rssi, s_system_data.snr);
}

/**
 * @brief 채널 리스트 문자열 생성 (가운데 정렬 + 생략)
 * @param channels 채널 배열
 * @param count 채널 수
 * @param buf 출력 버퍼
 * @param buf_size 버퍼 크기
 * @param max_width 최대 너비 (픽셀)
 * @param u8g2 U8g2 인스턴스 (너비 계산용)
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
            // trunc_width + ellipsis_width가 max_width를 넘지 않도록 조정
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

/**
 * @brief RX 수신 통계 페이지 그리기 (Page 3)
 */
static void draw_rx_stats_page(u8g2_t* u8g2)
{
    draw_rx_header(u8g2);

    // 헤더
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    u8g2_DrawStr(u8g2, 2, 10, "RX STATS");

    // 구분선
    u8g2_DrawHLine(u8g2, 0, 14, 128);

    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);

    // Last RSSI
    u8g2_DrawStr(u8g2, 2, 28, "RSSI:");
    char rssi_str[16];
    snprintf(rssi_str, sizeof(rssi_str), "%d dBm", s_rx_stats.last_rssi);
    u8g2_DrawStr(u8g2, 40, 28, rssi_str);

    // Last SNR
    u8g2_DrawStr(u8g2, 2, 39, "SNR:");
    char snr_str[16];
    snprintf(snr_str, sizeof(snr_str), "%.1f dB", s_rx_stats.last_snr);
    u8g2_DrawStr(u8g2, 40, 39, snr_str);

    // RX Interval (Tally 패킷 수신 간격, ms 단위)
    u8g2_DrawStr(u8g2, 2, 50, "INTVL:");
    char interval_str[16];
    if (s_rx_stats.interval >= 1000) {
        snprintf(interval_str, sizeof(interval_str), "%.1f s", s_rx_stats.interval / 1000.0f);
    } else {
        snprintf(interval_str, sizeof(interval_str), "%lu ms", (unsigned long)s_rx_stats.interval);
    }
    u8g2_DrawStr(u8g2, 40, 50, interval_str);
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

// ========== RX 통계 데이터 설정 ==========

extern "C" void rx_page_set_rx_stats(int16_t rssi, float snr, uint32_t interval)
{
    s_rx_stats.last_rssi = rssi;
    s_rx_stats.last_snr = snr;
    s_rx_stats.interval = interval;
}

// ========== 페이지 제어 ==========

extern "C" void rx_page_switch_page(uint8_t page)
{
    if (page >= 1 && page <= 3) {
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
    T_LOGD(TAG, "Camera ID popup shown (ID: %d, max: %d)", s_display_camera_id, max_camera_num);
}

extern "C" void rx_page_hide_camera_id_popup(void)
{
    s_page_state = RX_PAGE_STATE_NORMAL;
    s_camera_id_changing = false;
    T_LOGD(TAG, "Camera ID popup hidden");
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
    T_LOGD(TAG, "Camera ID cycled: %d (max: %d)", s_display_camera_id, max_camera_num);
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

/**
 * @brief 기능 정지 상태 팝업 그리기
 * 중앙에 "STOPPED" + "LICENSE REQUIRED" 메시지 표시
 */
static void draw_stopped_popup(u8g2_t* u8g2)
{
    // 팝업 박스 좌표 (더 큰 박스)
    int popup_x = 4;
    int popup_y = 12;
    int popup_w = 120;
    int popup_h = 44;

    // 이중 테두리 (흰색)
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, popup_x, popup_y, popup_w, popup_h);
    u8g2_DrawFrame(u8g2, popup_x + 2, popup_y + 2, popup_w - 4, popup_h - 4);

    // "STOPPED" 텍스트 (흰색, 중간 폰트)
    u8g2_SetFont(u8g2, u8g2_font_profont15_mf);
    const char* msg1 = "STOPPED";
    int msg1_width = u8g2_GetStrWidth(u8g2, msg1);
    u8g2_DrawStr(u8g2, (128 - msg1_width) / 2, popup_y + 18, msg1);

    // "LICENSE REQUIRED" 텍스트 (흰색, 작음)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    const char* msg2 = "LICENSE REQUIRED";
    int msg2_width = u8g2_GetStrWidth(u8g2, msg2);
    u8g2_DrawStr(u8g2, (128 - msg2_width) / 2, popup_y + 32, msg2);
}

// ============================================================================
// 기능 정지 상태 제어 API
// ============================================================================

/**
 * @brief 기능 정지 상태 설정
 * @param stopped true: 정지 상태, false: 정상 상태
 */
extern "C" void rx_page_set_stopped(bool stopped)
{
    s_stopped = stopped;
    if (stopped) {
        T_LOGW(TAG, "RxPage: Function stopped state set");
    } else {
        T_LOGI(TAG, "RxPage: Function stopped state cleared");
    }
}

/**
 * @brief 기능 정지 상태 확인
 * @return true: 정지 상태, false: 정상 상태
 */
extern "C" bool rx_page_is_stopped(void)
{
    return s_stopped;
}
