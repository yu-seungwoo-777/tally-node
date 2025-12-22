/**
 * @file button_actions.c
 * @brief 버튼 액션 맵핑 구현 - PageManager 연동
 * @author Claude Code
 * @date 2025-12-09
 */

#include "button_actions.h"
#include "log.h"
#include "log_tags.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "pages/SettingsPage.h"
#include "pages/RxPage.h"
#include "pages/TxPage.h"
#include <string.h>

// Forward declarations for PageManager functions
typedef enum {
    PAGE_TYPE_BOOT = 0,
    PAGE_TYPE_RX,
    PAGE_TYPE_TX,
    PAGE_TYPE_SETTINGS,
    PAGE_TYPE_NONE
} PageType_t;

extern esp_err_t PageManager_init(void);
extern void PageManager_handleButton(int button_id);
extern void PageManager_handleLongPress(int button_id);
extern void PageManager_handleLongPressRelease(int button_id);
extern PageType_t PageManager_getCurrentPage(void);
extern void PageManager_switchPage(PageType_t page_type);
extern void PageManager_enterSettings(void);
extern void PageManager_exitSettings(void);

static const char* TAG = TAG_BUTTON;


// 버튼 기능 구현
static void handle_single_click(void)
{
    // 현재 페이지 확인
    PageType_t current_page = PageManager_getCurrentPage();

    if (current_page == PAGE_TYPE_RX) {
        LOG_0(TAG, "버튼 클릭 - RxPage 페이지 전환");
        // RxPage의 페이지 전환 기능 직접 호출
        uint8_t current_rx_page = RxPage_getCurrentPage();
        uint8_t next_page = (current_rx_page == 1) ? 2 : 1;
        RxPage_switchPage(next_page);
        LOG_0(TAG, "RxPage %d페이지에서 %d페이지로 전환", current_rx_page, next_page);
    }
    #ifdef DEVICE_MODE_TX
    else if (current_page == PAGE_TYPE_TX) {
        LOG_0(TAG, "버튼 클릭 - TxPage 페이지 전환");
        // TxPage의 페이지 전환 기능 직접 호출
        uint8_t current_tx_page = TxPage_getCurrentPage();
        LOG_0(TAG, "현재 TX 페이지: %d", current_tx_page);
        uint8_t next_page = (current_tx_page == 3) ? 1 : (current_tx_page + 1);  // 1→2→3→1 순환
        LOG_0(TAG, "다음 TX 페이지: %d", next_page);
        TxPage_switchPage(next_page);
        LOG_0(TAG, "TxPage %d페이지에서 %d페이지로 전환 완료", current_tx_page, next_page);
    }
#endif
    else if (current_page == PAGE_TYPE_SETTINGS) {
        // 팩토리 리셋 확인 페이지 상태 확인
        if (SettingsPage_isInFactoryResetConfirm()) {
            LOG_0(TAG, "버튼 클릭 - 팩토리 리셋 확인 페이지에서 메뉴 선택");
            PageManager_handleButton(0);  // 확인 페이지에서 메뉴 선택
        } else {
            LOG_0(TAG, "버튼 클릭 - 설정 페이지 메뉴 이동");
            PageManager_handleButton(0);  // 설정 페이지 메뉴 이동
        }
    }
}

static void handle_long_press(void)
{
    // 현재 페이지 확인
    PageType_t current_page = PageManager_getCurrentPage();

    if (current_page == PAGE_TYPE_SETTINGS) {
        // Exit 메뉴가 선택된 경우에만 페이지 나가기
        if (SettingsPage_isExitSelected()) {
            LOG_0(TAG, "설정 페이지 Exit 선택 상태에서 롱프레스 - 페이지 나가기");
            PageManager_exitSettings();
        } else {
            LOG_0(TAG, "설정 페이지에서 롱프레스 - PageManager로 전달");
            // PageManager로 롱프레스 전달 (Factory Reset 처리)
            PageManager_handleLongPress(0);
        }
    } else {
        LOG_0(TAG, "길게 누르기 - 설정 페이지 진입");
        // 길게 누르면 설정 페이지 진입
        PageManager_enterSettings();
    }
}

static void handle_long_press_release(void)
{
    // 현재 페이지 확인
    PageType_t current_page = PageManager_getCurrentPage();

    if (current_page == PAGE_TYPE_SETTINGS) {
        LOG_0(TAG, "설정 페이지에서 롱프레스 해제 - PageManager로 전달");
        // PageManager로 롱프레스 해제 전달
        PageManager_handleLongPressRelease(0);
    }
}

// 버튼 기능 맵핑 테이블 (공통)
static const button_function_t button_functions[] = {
    { "SINGLE", "RxPage 페이지 전환", handle_single_click },
    { "LONG",   "설정 페이지 진입", handle_long_press },
    { "LONG_RELEASE", "롱프레스 해제", handle_long_press_release }
};

// 버튼 기능 맵핑 테이블 반환
const button_function_t* get_button_functions(size_t* count)
{
    *count = sizeof(button_functions) / sizeof(button_functions[0]);
    return button_functions;
}

// 버튼 액션 초기화
void button_actions_init(void)
{
    LOG_0(TAG, "버튼 액션 초기화 (PageManager 연동)");
}

// 버튼 액션 실행
void button_actions_execute(button_action_t action)
{
    switch (action) {
        case BUTTON_ACTION_SINGLE:
            handle_single_click();
            break;
        case BUTTON_ACTION_LONG:
            handle_long_press();
            break;
        case BUTTON_ACTION_LONG_RELEASE:
            handle_long_press_release();
            break;
        default:
            LOG_0(TAG, "알 수 없는 버튼 액션: %d", action);
            break;
    }
}

// 버튼 액션 핸들러 설정
void button_actions_set_handler(button_action_handler_t handler)
{
    // 현재는 핸들러 지원하지 않음
    LOG_0(TAG, "버튼 액션 핸들러 설정 (지원하지 않음)");
}