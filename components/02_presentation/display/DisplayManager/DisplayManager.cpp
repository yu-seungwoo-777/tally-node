/**
 * @file DisplayManager.cpp
 * @brief 디스플레이 관리자 구현
 */

#include "DisplayManager.h"
#include "DisplayDriver.h"
#include "BootPage.h"
#include "t_log.h"
#include <string.h>

// ============================================================================
// 상수
// ============================================================================

static const char* TAG = "DisplayMgr";
#define DEFAULT_REFRESH_INTERVAL_MS  200  // 5 FPS
#define MAX_PAGES                     5

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    bool initialized;
    bool running;
    bool power_on;
    uint32_t refresh_interval_ms;
    uint32_t last_refresh_ms;

    display_page_t current_page;
    display_page_t previous_page;

    // 등록된 페이지 목록
    const display_page_interface_t* pages[MAX_PAGES];
    int page_count;
} s_mgr = {
    .initialized = false,
    .running = false,
    .power_on = true,
    .refresh_interval_ms = DEFAULT_REFRESH_INTERVAL_MS,
    .last_refresh_ms = 0,
    .current_page = PAGE_NONE,
    .previous_page = PAGE_NONE,
    .pages = {nullptr},
    .page_count = 0,
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 현재 페이지 렌더링
 */
static void render_current_page(void)
{
    if (s_mgr.current_page == PAGE_NONE || s_mgr.current_page >= PAGE_COUNT) {
        return;
    }

    // 등록된 페이지에서 찾기
    for (int i = 0; i < s_mgr.page_count; i++) {
        if (s_mgr.pages[i]->id == s_mgr.current_page) {
            u8g2_t* u8g2 = display_manager_get_u8g2();
            if (u8g2 != nullptr) {
                DisplayDriver_clearBuffer();
                s_mgr.pages[i]->render(u8g2);
                DisplayDriver_sendBuffer();
            }
            break;
        }
    }
}

/**
 * @brief 페이지 전환 처리
 */
static void handle_page_transition(void)
{
    // 이전 페이지의 on_exit 호출
    if (s_mgr.previous_page != PAGE_NONE && s_mgr.previous_page != s_mgr.current_page) {
        for (int i = 0; i < s_mgr.page_count; i++) {
            if (s_mgr.pages[i]->id == s_mgr.previous_page) {
                if (s_mgr.pages[i]->on_exit != nullptr) {
                    s_mgr.pages[i]->on_exit();
                }
                break;
            }
        }
    }

    // 새 페이지의 on_enter 호출
    if (s_mgr.current_page != PAGE_NONE) {
        for (int i = 0; i < s_mgr.page_count; i++) {
            if (s_mgr.pages[i]->id == s_mgr.current_page) {
                if (s_mgr.pages[i]->on_enter != nullptr) {
                    s_mgr.pages[i]->on_enter();
                }
                break;
            }
        }
    }
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool display_manager_init(void)
{
    if (s_mgr.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return true;
    }

    // DisplayDriver 초기화
    esp_err_t ret = DisplayDriver_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "DisplayDriver 초기화 실패: %s", esp_err_to_name(ret));
        return false;
    }

    memset(s_mgr.pages, 0, sizeof(s_mgr.pages));
    s_mgr.page_count = 0;
    s_mgr.current_page = PAGE_NONE;
    s_mgr.previous_page = PAGE_NONE;

    // BootPage 자동 등록
    boot_page_init();

    s_mgr.initialized = true;
    T_LOGI(TAG, "DisplayManager 초기화 완료");
    return true;
}

extern "C" void display_manager_start(void)
{
    if (!s_mgr.initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return;
    }

    s_mgr.running = true;
    T_LOGI(TAG, "DisplayManager 시작");
}

extern "C" void display_manager_set_refresh_interval(uint32_t interval_ms)
{
    s_mgr.refresh_interval_ms = interval_ms;
}

extern "C" bool display_manager_register_page(const display_page_interface_t* page_interface)
{
    if (page_interface == nullptr) {
        T_LOGE(TAG, "페이지 인터페이스가 nullptr임");
        return false;
    }

    if (s_mgr.page_count >= MAX_PAGES) {
        T_LOGE(TAG, "페이지 등록 한도 도달 (%d)", MAX_PAGES);
        return false;
    }

    // 중복 확인
    for (int i = 0; i < s_mgr.page_count; i++) {
        if (s_mgr.pages[i]->id == page_interface->id) {
            T_LOGW(TAG, "페이지 ID %d 이미 등록됨", page_interface->id);
            return false;
        }
    }

    s_mgr.pages[s_mgr.page_count++] = page_interface;
    T_LOGI(TAG, "페이지 등록: %s (ID=%d)", page_interface->name, page_interface->id);

    // 페이지 초기화
    if (page_interface->init != nullptr) {
        page_interface->init();
    }

    return true;
}

extern "C" void display_manager_set_page(display_page_t page_id)
{
    if (!s_mgr.initialized) {
        return;
    }

    if (s_mgr.current_page == page_id) {
        return;  // 이미 같은 페이지
    }

    s_mgr.previous_page = s_mgr.current_page;
    s_mgr.current_page = page_id;

    T_LOGI(TAG, "페이지 전환: %d -> %d", s_mgr.previous_page, s_mgr.current_page);

    handle_page_transition();
    display_manager_force_refresh();
}

extern "C" display_page_t display_manager_get_current_page(void)
{
    return s_mgr.current_page;
}

extern "C" void display_manager_force_refresh(void)
{
    if (!s_mgr.initialized || !s_mgr.power_on) {
        return;
    }

    // 즉시 렌더링
    render_current_page();
}

extern "C" void display_manager_set_power(bool on)
{
    s_mgr.power_on = on;
    DisplayDriver_setPower(on);

    if (on && s_mgr.initialized) {
        display_manager_force_refresh();
    }

    T_LOGI(TAG, "디스플레이 전원: %s", on ? "ON" : "OFF");
}

extern "C" u8g2_t* display_manager_get_u8g2(void)
{
    return DisplayDriver_getU8g2();
}

// ============================================================================
// BootPage 편의 API 구현
// ============================================================================

extern "C" void display_manager_boot_set_message(const char* message)
{
    boot_page_set_message(message);
}

extern "C" void display_manager_boot_set_progress(uint8_t progress)
{
    boot_page_set_progress(progress);
}

/**
 * @brief 디스플레이 갱신 루프 (주기적으로 호출해야 함)
 *
 * @note 이 함수는 메인 루프나 타이머에서 주기적으로 호출되어야 함
 */
extern "C" void display_manager_update(void)
{
    if (!s_mgr.initialized || !s_mgr.running || !s_mgr.power_on) {
        return;
    }

    // 갱신 주기 체크
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - s_mgr.last_refresh_ms < s_mgr.refresh_interval_ms) {
        return;
    }

    s_mgr.last_refresh_ms = now;
    render_current_page();
}
