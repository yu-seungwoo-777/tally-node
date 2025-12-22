/**
 * @file BootScreen.c
 * @brief TALLY-NODE Boot Screen Manager
 *
 * TALLY-NODE 전용 부팅 화면 관리
 * 프로페셔널 디자인의 부팅 화면 구현
 */

#include "pages/BootScreen.h"
#include "core/DisplayHelper.h"
#include "u8g2.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>


// 상태 변수
static bool s_initialized = false;
static bool s_boot_complete = false;

// 부트 스테이지 현재 메시지
static char s_current_message[64] = "System Startup";

// BootScreen 내부 함수들
static void drawProfessionalBox(void);

// BootScreen가 관리하는 함수들
esp_err_t BootScreen_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // DisplayManager에서 U8g2 인스턴스 가져오기 시도
    // 아직 초기화되지 않았을 수 있으므로 여기서는 초기화하지 않음
    // 실제 사용 시점에서 인스턴스를 가져옴

    s_initialized = true;
    s_boot_complete = false;
    return ESP_OK;
}


// 프로페셔널 박스 그리기
static void drawProfessionalBox(void)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 프로페셔널 박스 (가운데 정렬)
    const int box_width = 124; // 너비
    const int box_height = 34; // 2줄 텍스트를 위한 높이
    const int box_x = (128 - box_width) / 2;  // 가운데 정렬
    const int box_y = 2;

    // 박스 그리기 (두꺼운 테두리, +2 간격)
    u8g2_DrawFrame(u8g2, box_x, box_y, box_width, box_height);
    u8g2_DrawFrame(u8g2, box_x + 2, box_y + 2, box_width - 4, box_height - 4);

    // 타이틀과 버전 (1줄, 중앙 정렬)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    const char* title_version = "TALLY-NODE v2.0.0";
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

// BootScreen 관련 함수들 - TALLY-NODE 프로페셔널 디자인
void BootScreen_showBootScreen(void)
{
    if (!s_initialized) return;

    s_boot_complete = false;

    // 초기 부트 화면
    DisplayHelper_clearBuffer();

    // 프로페셔널 박스
    drawProfessionalBox();

    DisplayHelper_sendBuffer();
}


void BootScreen_showBootMessage(const char* message, int progress, int delay_ms)
{
    if (!s_initialized || s_boot_complete || !message) return;

    // 메시지 업데이트
    strncpy(s_current_message, message, sizeof(s_current_message) - 1);
    s_current_message[sizeof(s_current_message) - 1] = '\0';

    // U8g2 인스턴스 가져오기
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 화면 업데이트
    DisplayHelper_clearBuffer();

    // 프로페셔널 박스
    drawProfessionalBox();

    // 진행 문구와 퍼센트 표시 (1줄)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char percent_text[12];
    snprintf(percent_text, sizeof(percent_text), "%d%%", progress);

    // 문구와 퍼센트 결합
    char combined_text[80];
    snprintf(combined_text, sizeof(combined_text), "%s %s", s_current_message, percent_text);
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
    int fill_width = (bar_width * progress) / 100;
    if (fill_width > 0) {
        u8g2_DrawBox(u8g2, bar_x, bar_y, fill_width, bar_height);
    }

    DisplayHelper_sendBuffer();

    // 지정된 딜레이 시간만큼 대기
    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void BootScreen_bootComplete(bool success, const char* message)
{
    if (!s_initialized) return;

    s_boot_complete = true;

    // U8g2 인스턴스 가져오기
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 완료 화면
    DisplayHelper_clearBuffer();

    // 프로페셔널 박스
    drawProfessionalBox();

    // 완료 메시지 표시
    const char* complete_msg = message ? message : (success ? "System Ready" : "Boot Failed!");

    // 메시지와 퍼센트 결합
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    char combined_text[80];
    if (success) {
        snprintf(combined_text, sizeof(combined_text), "%s 100%%", complete_msg);
    } else {
        snprintf(combined_text, sizeof(combined_text), "%s", complete_msg);
    }
    int msg_width = u8g2_GetStrWidth(u8g2, combined_text);
    int msg_x = (128 - msg_width) / 2;
    u8g2_DrawStr(u8g2, msg_x, 50, combined_text);

    // 프로그레스바 (전체 너비)
    const int bar_width = 112;  // 전체 너비에서 좌우 여백 8px씩
    const int bar_height = 6;
    const int bar_x = 8;
    const int bar_y = 56;

    // 프로그레스바 (완전히 채움)
    if (success) {
        u8g2_DrawBox(u8g2, bar_x, bar_y, bar_width, bar_height);
    } else {
        u8g2_DrawFrame(u8g2, bar_x, bar_y, bar_width, bar_height);
    }

    DisplayHelper_sendBuffer();

    // 2초 후에 일반 화면으로 전환
    if (success) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        // DisplayManager_showNormalScreen() 호출 필요
    }
}

