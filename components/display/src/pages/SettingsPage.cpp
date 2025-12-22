/**
 * @file SettingsPage.c
 * @brief TALLY-NODE Settings Page Implementation
 *
 * 설정 페이지 UI 렌더링 및 사용자 입력 처리
 */

#include "pages/SettingsPage.h"
#include "core/DisplayHelper.h"
#include "u8g2.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"  // esp_restart를 위해 추가
#include <string.h>
#include <stdio.h>  // snprintf을 위해 추가
#include "ConfigCore.h"  // ConfigCore 사용을 위해 추가

static const char* TAG = "SETTINGS";

// 상태 변수
static bool s_initialized = false;
static bool s_visible = false;

// 롱프레스 관련 변수
static bool s_long_press_active = false;
static uint32_t s_long_press_start_time = 0;
static int s_countdown_seconds = 10;

// 페이지 상태
typedef enum {
    PAGE_STATE_MENU = 0,         // 메뉴 선택 상태
    PAGE_STATE_COUNTDOWN,        // 카운트다운 상태
#ifdef DEVICE_MODE_RX
    PAGE_STATE_CAMERA_ID         // Camera ID 변경 상태 (RX 전용)
#endif
} page_state_t;

static page_state_t s_page_state = PAGE_STATE_MENU;

#ifdef DEVICE_MODE_RX
// Camera ID 관련 변수 (RX 전용)
static int s_current_camera_id = 0;     // 현재 표시되는 ID
static int s_original_camera_id = 0;    // 원래 저장된 ID
static bool s_camera_id_changing = false;  // ID 변경 중인지
static uint32_t s_camera_id_change_time = 0;  // 마지막 변경 시간
#endif

// 메뉴 상태
typedef enum {
#ifdef DEVICE_MODE_RX
    MENU_ITEM_CAMERA_ID = 0,    // RX 전용
    MENU_ITEM_FACTORY_RESET,    // 공통
#else
    MENU_ITEM_FACTORY_RESET = 0, // TX 전용 시작
#endif
    MENU_ITEM_EXIT,             // 공통
    MENU_ITEM_COUNT
} menu_item_t;

static menu_item_t s_current_menu;

// 메뉴 초기화 함수
static void initMenu(void) {
#ifdef DEVICE_MODE_RX
    s_current_menu = MENU_ITEM_CAMERA_ID;  // RX 모드는 Camera ID부터 시작
#else
    s_current_menu = MENU_ITEM_FACTORY_RESET;  // TX 모드는 Factory Reset부터 시작
#endif
}

// SettingsPage 내부 함수들
static void drawSettingsLayout(void);
static void drawCountdown(void);
#ifdef DEVICE_MODE_RX
static void drawCameraIdPopup(void);
#endif
static void executeFactoryReset(void);
static uint32_t getTickMs(void);

// 카운트다운 및 Camera ID 자동 변경을 위한 타이머 함수
static void startCountdownTimer(void);
static void stopCountdownTimer(void);
#ifdef DEVICE_MODE_RX
static void startCameraIdAutoChange(void);
static void stopCameraIdAutoChange(void);
#endif

// 타이머 핸들
static TimerHandle_t s_countdown_timer = nullptr;
#ifdef DEVICE_MODE_RX
static TimerHandle_t s_camera_id_timer = nullptr;
#endif

// 타이머 콜백 함수
static void countdownTimerCallback(TimerHandle_t xTimer);
#ifdef DEVICE_MODE_RX
static void cameraIdTimerCallback(TimerHandle_t xTimer);
#endif

// SettingsPage가 관리하는 함수들
esp_err_t SettingsPage_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // DisplayManager에서 U8g2 인스턴스를 사용하므로 여기서는 초기화하지 않음

    s_initialized = true;
    s_visible = false;

    // 메뉴 초기화
    initMenu();

#ifdef DEVICE_MODE_RX
    // Camera ID 초기화 - ConfigCore에서 로드 (RX 전용)
    s_current_camera_id = ConfigCore::getCameraId();
    s_original_camera_id = s_current_camera_id;

    // Camera ID가 0이면 1로 설정 (0은 사용하지 않음)
    if (s_current_camera_id == 0) {
        s_current_camera_id = 1;
        s_original_camera_id = 1;
        // ConfigCore에 저장
        ConfigCore::setCameraId(1);
    }

    LOG_0(TAG, "ConfigCore에서 Camera ID 로드: %d", s_current_camera_id);
    LOG_0(TAG, "설정 페이지 초기화: 현재 메뉴=%d, Camera ID=%d", s_current_menu, s_current_camera_id);
#else
    LOG_0(TAG, "설정 페이지 초기화: 현재 메뉴=%d", s_current_menu);
#endif

    return ESP_OK;
}

void SettingsPage_showPage(void)
{
    if (!s_initialized) return;

    s_visible = true;
    s_page_state = PAGE_STATE_MENU;  // 메뉴 상태로 초기화
    s_long_press_active = false;

    // 화면 지우기
    DisplayHelper_clearBuffer();

    // 현재 페이지 상태에 따른 레이아웃 그리기
    switch (s_page_state) {
        case PAGE_STATE_MENU:
            drawSettingsLayout();
            break;
        case PAGE_STATE_COUNTDOWN:
            drawCountdown();
            break;
#ifdef DEVICE_MODE_RX
        case PAGE_STATE_CAMERA_ID:
            drawCameraIdPopup();
            break;
#endif
    }

    // 버퍼 전송
    DisplayHelper_sendBuffer();
}

void SettingsPage_hidePage(void)
{
    if (!s_initialized) return;

    s_visible = false;

    DisplayHelper_clearBuffer();
    DisplayHelper_sendBuffer();
}

void SettingsPage_handleButton(int button_id)
{
    if (!s_initialized || !s_visible) return;

    switch (s_page_state) {
        case PAGE_STATE_MENU:
            // 메뉴 상태: 단클릭으로 메뉴 이동
            s_current_menu = static_cast<menu_item_t>(s_current_menu + 1);

            // 모드에 따른 메뉴 범위 체크
#ifdef DEVICE_MODE_RX
            // RX 모드: Camera ID -> Factory Reset -> Exit -> Camera ID (순환)
            if (s_current_menu >= MENU_ITEM_COUNT) {
                s_current_menu = MENU_ITEM_CAMERA_ID;
            }
#else
            // TX 모드: Factory Reset -> Exit -> Factory Reset (순환)
            if (s_current_menu >= MENU_ITEM_COUNT) {
                s_current_menu = MENU_ITEM_FACTORY_RESET;
            }
#endif

            LOG_0(TAG, "메뉴 이동: %d, Factory Reset=%d, Exit=%d", s_current_menu, MENU_ITEM_FACTORY_RESET, MENU_ITEM_EXIT);
            break;

        case PAGE_STATE_COUNTDOWN:
            // 카운트다운 상태: 취소
            s_page_state = PAGE_STATE_MENU;
            s_long_press_active = false;

            // 카운트다운 타이머 중지
            stopCountdownTimer();

            // 메뉴 상태 유지 (Factory Reset 선택된 상태로)
            LOG_0(TAG, "팩토리 리셋 취소 (클릭) - 메뉴=%d", s_current_menu);
            break;

#ifdef DEVICE_MODE_RX
        case PAGE_STATE_CAMERA_ID:
            // Camera ID 변경 상태: 메뉴로 복귀
            s_page_state = PAGE_STATE_MENU;
            s_camera_id_changing = false;

            // Camera ID 자동 변경 타이머 중지
            stopCameraIdAutoChange();

            // 메뉴 상태 유지 (Camera ID 선택된 상태로)
            LOG_0(TAG, "Camera ID 변경 취소 (클릭) - 메뉴=%d", s_current_menu);
            break;
#endif
    }

    // 화면 즉시 업데이트
    DisplayHelper_clearBuffer();
    switch (s_page_state) {
        case PAGE_STATE_MENU:
            LOG_0(TAG, "화면 업데이트: 메뉴 페이지");
            drawSettingsLayout();
            break;
        case PAGE_STATE_COUNTDOWN:
            LOG_0(TAG, "화면 업데이트: 카운트다운 페이지");
            drawCountdown();
            break;

#ifdef DEVICE_MODE_RX
        case PAGE_STATE_CAMERA_ID:
            LOG_0(TAG, "화면 업데이트: Camera ID 페이지");
            drawCameraIdPopup();
            break;
#endif
    }
    LOG_0(TAG, "DisplayHelper_sendBuffer() 호출");
    DisplayHelper_sendBuffer();
    LOG_0(TAG, "DisplayHelper_sendBuffer() 호출 완료");
}

void SettingsPage_update(void)
{
    if (!s_initialized || !s_visible) return;

    // 롱프레스 카운트다운 처리
    if (s_long_press_active && s_page_state == PAGE_STATE_COUNTDOWN) {
        uint32_t current_time = getTickMs();
        uint32_t elapsed = current_time - s_long_press_start_time;

        int new_countdown = 10 - (elapsed / 1000);

        LOG_1(TAG, "카운트다운: 경과=%lums, 남은=%d초, 현재=%d초", elapsed, new_countdown, s_countdown_seconds);

        if (new_countdown != s_countdown_seconds) {
            s_countdown_seconds = new_countdown;
            LOG_0(TAG, "카운트다운 업데이트: %d초", s_countdown_seconds);

            if (s_countdown_seconds <= 0) {
                // 카운트다운 완료, 팩토리 리셋 실행
                executeFactoryReset();
            } else {
                // 화면 업데이트
                DisplayHelper_clearBuffer();
                drawCountdown();
                DisplayHelper_sendBuffer();
            }
        }
    }

    #ifdef DEVICE_MODE_RX
    // Camera ID 자동 변경 처리 (RX 전용)
    if (s_camera_id_changing && s_page_state == PAGE_STATE_CAMERA_ID) {
        uint32_t current_time = getTickMs();
        uint32_t elapsed = current_time - s_camera_id_change_time;

        LOG_1(TAG, "Camera ID 체크: 경과=%lums, 다음 변경까지=%d", elapsed, 800 - elapsed);

        // 0.8초마다 ID 변경
        if (elapsed >= 800) {
            // max_camera_num 값 가져오기
            uint8_t max_camera = ConfigCore::getMaxCameraNum();

            // 1부터 max_camera_num까지 순환 (0 제외)
            s_current_camera_id++;
            if (s_current_camera_id > max_camera) {
                s_current_camera_id = 1;  // 1로 리셋
            }
            s_camera_id_change_time = current_time;

            LOG_0(TAG, "Camera ID 변경: %d (최대: %d)", s_current_camera_id, max_camera);

            // 화면 업데이트
            DisplayHelper_clearBuffer();
            drawCameraIdPopup();
            DisplayHelper_sendBuffer();
        }
    }
#endif
}

// 현재 선택된 메뉴가 Exit인지 확인
bool SettingsPage_isExitSelected(void)
{
    if (!s_initialized || !s_visible) {
        return false;
    }

    return (s_current_menu == MENU_ITEM_EXIT);
}

// 현재 팩토리 리셋 카운트다운 상태인지 확인
bool SettingsPage_isInFactoryResetConfirm(void)
{
    if (!s_initialized || !s_visible) {
        return false;
    }

    return (s_page_state == PAGE_STATE_COUNTDOWN);
}

// 현재 Camera ID 변경 중인지 확인 (RX 전용)
bool SettingsPage_isInCameraIdChange(void)
{
    if (!s_initialized || !s_visible) {
        return false;
    }

#ifdef DEVICE_MODE_RX
    return (s_page_state == PAGE_STATE_CAMERA_ID && s_camera_id_changing);
#else
    return false;
#endif
}

// 설정 레이아웃 그리기
static void drawSettingsLayout(void)
{
    // U8g2 인스턴스 가져오기
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 왼쪽 위 제목 표시
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    const char* title = "SETTINGS";
    u8g2_DrawStr(u8g2, 5, 12, title);

    // 구분선
    u8g2_DrawLine(u8g2, 5, 16, 123, 16);

    // 메뉴 항목
    int y_pos = 30;

    // RX 모드 메뉴
#ifdef DEVICE_MODE_RX
    // Camera ID
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);  // 각 텍스트 그리기 직전 폰트 설정
    if (s_current_menu == MENU_ITEM_CAMERA_ID) {
        // 선택된 항목은 반전 표시
        u8g2_DrawBox(u8g2, 3, y_pos - 10, 122, 12);
        u8g2_SetDrawColor(u8g2, 0);  // 흰색 글자
        u8g2_DrawStr(u8g2, 5, y_pos, "> Camera ID Change");
        u8g2_SetDrawColor(u8g2, 1);  // 검은색으로 복원
    } else {
        u8g2_DrawStr(u8g2, 5, y_pos, "  Camera ID Change");
    }
    y_pos += 14;  // 메뉴 간격 2픽셀 증가 (12 -> 14)
#endif

    // Factory Reset
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);  // 각 텍스트 그리기 직전 폰트 설정
    if (s_current_menu == MENU_ITEM_FACTORY_RESET) {
        // 선택된 항목은 반전 표시
        u8g2_DrawBox(u8g2, 3, y_pos - 10, 122, 12);
        u8g2_SetDrawColor(u8g2, 0);  // 흰색 글자
        u8g2_DrawStr(u8g2, 5, y_pos, "> Factory Reset");
        u8g2_SetDrawColor(u8g2, 1);  // 검은색으로 복원
    } else {
        u8g2_DrawStr(u8g2, 5, y_pos, "  Factory Reset");
    }
    y_pos += 14;  // 메뉴 간격 2픽셀 증가 (12 -> 14)

    // Exit
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);  // 각 텍스트 그리기 직전 폰트 설정
    if (s_current_menu == MENU_ITEM_EXIT) {
        // 선택된 항목은 반전 표시
        u8g2_DrawBox(u8g2, 3, y_pos - 10, 122, 12);
        u8g2_SetDrawColor(u8g2, 0);  // 흰색 글자
        u8g2_DrawStr(u8g2, 5, y_pos, "> Exit");
        u8g2_SetDrawColor(u8g2, 1);  // 검은색으로 복원
    } else {
        u8g2_DrawStr(u8g2, 5, y_pos, "  Exit");
    }
}

// 카운트다운 화면 그리기
static void drawCountdown(void)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 전체 배경
    u8g2_SetDrawColor(u8g2, 1);  // 검은색

    // 팝업 박스 (최외곽 테두리 포함)
    int popup_x = 2;
    int popup_y = 2;
    int popup_w = 124;
    int popup_h = 60;

    // 팝업 배경 (흰색)
    u8g2_SetDrawColor(u8g2, 0);  // 반전: 흰색 배경
    u8g2_DrawBox(u8g2, popup_x, popup_y, popup_w, popup_h);

    // 팝업 테두리 (2줄 - 최외곽)
    u8g2_SetDrawColor(u8g2, 1);  // 검은색 테두리
    u8g2_DrawFrame(u8g2, popup_x, popup_y, popup_w, popup_h);
    u8g2_DrawFrame(u8g2, popup_x + 1, popup_y + 1, popup_w - 2, popup_h - 2);

    // 상단 제목 (검은색)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    int title_width = u8g2_GetStrWidth(u8g2, "FACTORY RESET");
    u8g2_DrawStr(u8g2, (128 - title_width) / 2, popup_y + 15, "FACTORY RESET");

    // 구분선 (더 길게)
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 22, popup_w - 10);
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 23, popup_w - 10);

    // 카운트다운 표시 (중앙 큰 글자, 검은색) - 아래로 이동
    char countdown_str[16];
    snprintf(countdown_str, sizeof(countdown_str), "%d", s_countdown_seconds);

    u8g2_SetFont(u8g2, u8g2_font_profont29_mn);
    int cd_width = u8g2_GetStrWidth(u8g2, countdown_str);
    u8g2_DrawStr(u8g2, (128 - cd_width) / 2, popup_y + 50, countdown_str);
}

// 롱프레스 시작 처리
void SettingsPage_handleLongPress(int button_id)
{
    if (!s_initialized || !s_visible) return;

    if (s_page_state == PAGE_STATE_MENU) {
#ifdef DEVICE_MODE_RX
        // RX 모드: Camera ID 메뉴 처리
        if (s_current_menu == MENU_ITEM_CAMERA_ID) {
            // Camera ID 선택 후 롱프레스 - ID 변경 팝업으로 이동
            s_page_state = PAGE_STATE_CAMERA_ID;
            s_original_camera_id = s_current_camera_id;  // 원래 ID 백업
            s_camera_id_changing = true;  // 즉시 번호 변경 시작
            s_camera_id_change_time = getTickMs();  // 시작 시간 기록
            LOG_0(TAG, "Camera ID 팝업으로 이동 (현재 ID: %d)", s_current_camera_id);

            // 화면 즉시 업데이트
            DisplayHelper_clearBuffer();
            drawCameraIdPopup();
            DisplayHelper_sendBuffer();

            // 카메라 ID 자동 변경 시작
            startCameraIdAutoChange();
        }
#endif
        // Factory Reset 메뉴 처리
        if (s_current_menu == MENU_ITEM_FACTORY_RESET) {
            // 메뉴에서 Factory Reset 선택 후 롱프레스 - 바로 카운트다운 시작
            s_page_state = PAGE_STATE_COUNTDOWN;
            s_long_press_active = true;
            s_countdown_seconds = 10;
            s_long_press_start_time = getTickMs();

            LOG_0(TAG, "팩토리 리셋 카운트다운 시작");

            // 화면 즉시 업데이트
            DisplayHelper_clearBuffer();
            drawCountdown();
            DisplayHelper_sendBuffer();

            // 카운트다운 타이머 시작
            startCountdownTimer();
        }
    }
#ifdef DEVICE_MODE_RX
    else if (s_page_state == PAGE_STATE_CAMERA_ID) {
        // Camera ID 팝업에서 롱프레스 - 이미 시작되었으므로 아무것도 안 함
        LOG_1(TAG, "Camera ID 롱프레스 진행 중");
    }
#endif
}

// 롱프레스 해제 처리
void SettingsPage_handleLongPressRelease(int button_id)
{
    if (!s_initialized || !s_visible) return;

    // 카운트다운 상태에서 롱프레스 해제 시 취소
    if (s_page_state == PAGE_STATE_COUNTDOWN && s_long_press_active) {
        s_page_state = PAGE_STATE_MENU;  // 메뉴로 바로 복귀
        s_long_press_active = false;

        // 카운트다운 타이머 중지
        stopCountdownTimer();

        // 메뉴 상태 유지 (초기화하지 않음)
        LOG_0(TAG, "팩토리 리셋 취소 (롱프레스 해제) - 메뉴로 복귀 (메뉴=%d)", s_current_menu);

        // 화면 즉시 업데이트
        DisplayHelper_clearBuffer();
        drawSettingsLayout();
        DisplayHelper_sendBuffer();
    }

    #ifdef DEVICE_MODE_RX
    // Camera ID 변경 상태에서 롱프레스 해제 시 저장 (RX 전용)
    if (s_page_state == PAGE_STATE_CAMERA_ID && s_camera_id_changing) {
        s_camera_id_changing = false;

        // Camera ID 자동 변경 타이머 중지
        stopCameraIdAutoChange();

        // ID가 변경되었으면 저장
        if (s_current_camera_id != s_original_camera_id) {
            // ConfigCore에 Camera ID 저장
            LOG_0(TAG, "Camera ID 저장: %d -> %d", s_original_camera_id, s_current_camera_id);
            ConfigCore::setCameraId(s_current_camera_id);
        }

        s_page_state = PAGE_STATE_MENU;
        LOG_0(TAG, "Camera ID 변경 완료 - 메뉴로 복귀");

        // 화면 즉시 업데이트
        DisplayHelper_clearBuffer();
        drawSettingsLayout();
        DisplayHelper_sendBuffer();
    }
#endif
}

// 팩토리 리셋 실행
static void executeFactoryReset(void)
{
    LOG_0(TAG, "팩토리 리셋 실행!");

    // 여기에 실제 팩토리 리셋 로직 구현
    // 예: NVS 삭제, 설정 초기화 등

    // 시스템 재부팅
    esp_restart();
}

#ifdef DEVICE_MODE_RX
// Camera ID 팝업 그리기 (RX 전용)
static void drawCameraIdPopup(void)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (!u8g2) return;

    // 전체 배경
    u8g2_SetDrawColor(u8g2, 1);  // 검은색

    // 팝업 박스 (최외곽 테두리 포함)
    int popup_x = 2;
    int popup_y = 2;
    int popup_w = 124;
    int popup_h = 60;

    // 팝업 배경 (흰색)
    u8g2_SetDrawColor(u8g2, 0);  // 반전: 흰색 배경
    u8g2_DrawBox(u8g2, popup_x, popup_y, popup_w, popup_h);

    // 팝업 테두리 (2줄 - 최외곽)
    u8g2_SetDrawColor(u8g2, 1);  // 검은색 테두리
    u8g2_DrawFrame(u8g2, popup_x, popup_y, popup_w, popup_h);
    u8g2_DrawFrame(u8g2, popup_x + 1, popup_y + 1, popup_w - 2, popup_h - 2);

    // 상단 제목 (검은색)
    u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
    int title_width = u8g2_GetStrWidth(u8g2, "CAMERA ID");
    u8g2_DrawStr(u8g2, (128 - title_width) / 2, popup_y + 15, "CAMERA ID");

    // 구분선 (더 길게)
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 22, popup_w - 10);
    u8g2_DrawHLine(u8g2, popup_x + 5, popup_y + 23, popup_w - 10);

    // ID 표시 (중앙 큰 글자, 검은색) - 아래로 이동
    char id_str[8];
    snprintf(id_str, sizeof(id_str), "%d", s_current_camera_id);

    u8g2_SetFont(u8g2, u8g2_font_profont29_mn);
    int id_width = u8g2_GetStrWidth(u8g2, id_str);
    u8g2_DrawStr(u8g2, (128 - id_width) / 2, popup_y + 50, id_str);
}
#endif

// 카운트다운 타이머 시작
static void startCountdownTimer(void)
{
    if (s_countdown_timer == nullptr) {
        s_countdown_timer = xTimerCreate(
            "countdown_timer",
            pdMS_TO_TICKS(1000),  // 1초 주기
            pdTRUE,              // 반복
            nullptr,
            countdownTimerCallback
        );
    }

    if (s_countdown_timer != nullptr && xTimerStart(s_countdown_timer, 0) == pdPASS) {
        LOG_0(TAG, "카운트다운 타이머 시작");
    }
}

// 카운트다운 타이머 중지
static void stopCountdownTimer(void)
{
    if (s_countdown_timer != nullptr) {
        xTimerStop(s_countdown_timer, 0);
        LOG_0(TAG, "카운트다운 타이머 중지");
    }
}

// 카운트다운 타이머 콜백
static void countdownTimerCallback(TimerHandle_t xTimer)
{
    if (!s_initialized || !s_visible) return;

    if (s_long_press_active && s_page_state == PAGE_STATE_COUNTDOWN) {
        s_countdown_seconds--;

        LOG_0(TAG, "카운트다운: %d초", s_countdown_seconds);

        if (s_countdown_seconds <= 0) {
            // 카운트다운 완료
            stopCountdownTimer();
            executeFactoryReset();
        } else {
            // 화면 업데이트
            DisplayHelper_clearBuffer();
            drawCountdown();
            DisplayHelper_sendBuffer();
        }
    }
}

#ifdef DEVICE_MODE_RX
// Camera ID 자동 변경 타이머 시작
static void startCameraIdAutoChange(void)
{
    if (s_camera_id_timer == nullptr) {
        s_camera_id_timer = xTimerCreate(
            "camera_id_timer",
            pdMS_TO_TICKS(800),   // 0.8초 주기
            pdTRUE,              // 반복
            nullptr,
            cameraIdTimerCallback
        );
    }

    if (s_camera_id_timer != nullptr && xTimerStart(s_camera_id_timer, 0) == pdPASS) {
        LOG_0(TAG, "Camera ID 자동 변경 타이머 시작");
    }
}

// Camera ID 자동 변경 타이머 중지
static void stopCameraIdAutoChange(void)
{
    if (s_camera_id_timer != nullptr) {
        xTimerStop(s_camera_id_timer, 0);
        LOG_0(TAG, "Camera ID 자동 변경 타이머 중지");
    }
}

// Camera ID 타이머 콜백
static void cameraIdTimerCallback(TimerHandle_t xTimer)
{
    if (!s_initialized || !s_visible) return;

    if (s_camera_id_changing && s_page_state == PAGE_STATE_CAMERA_ID) {
        // max_camera_num 값 가져오기
        uint8_t max_camera = ConfigCore::getMaxCameraNum();

        // 1부터 max_camera_num까지 순환 (0 제외)
        s_current_camera_id++;
        if (s_current_camera_id > max_camera) {
            s_current_camera_id = 1;  // 1로 리셋
        }

        LOG_0(TAG, "Camera ID 변경: %d (최대: %d)", s_current_camera_id, max_camera);

        // 화면 업데이트
        DisplayHelper_clearBuffer();
        drawCameraIdPopup();
        DisplayHelper_sendBuffer();
    }
}
#endif

// 현재 시간 가져오기 (ms)
static uint32_t getTickMs(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}