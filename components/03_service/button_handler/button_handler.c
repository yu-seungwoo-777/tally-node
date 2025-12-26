/**
 * @file button_handler.c
 * @brief 버튼 핸들러 구현
 *
 * button_poll 서비스의 이벤트를 받아서
 * DisplayManager의 페이지 전환 기능을 수행합니다.
 */

#include "button_handler.h"
#include "button_poll.h"
#include "DisplayManager.h"
#include "TxPage.h"
#include "RxPage.h"
#include "t_log.h"

static const char* TAG = "BtnHandler";

static bool s_started = false;

/**
 * @brief 버튼 콜백 함수
 *
 * @param action 발생한 버튼 액션
 */
static void button_callback(button_action_t action)
{
    display_page_t current_page = display_manager_get_current_page();

    switch (action) {
        case BUTTON_ACTION_SINGLE:
            // 단일 클릭: 현재 페이지 내에서 서브 페이지 전환
            if (current_page == PAGE_RX) {
                // RxPage: 1 ↔ 2 토글
                uint8_t current = rx_page_get_current_page();
                uint8_t next = (current == 1) ? 2 : 1;
                rx_page_switch_page(next);
                display_manager_force_refresh();
                T_LOGI(TAG, "RxPage: %d -> %d", current, next);
            }
            else if (current_page == PAGE_TX) {
                // TxPage: 1 -> 2 -> 3 -> 4 -> 5 -> 1 순환
                uint8_t current = tx_page_get_current_page();
                uint8_t next = (current == 5) ? 1 : (current + 1);
                tx_page_switch_page(next);
                display_manager_force_refresh();
                T_LOGI(TAG, "TxPage: %d -> %d", current, next);
            }
            else {
                T_LOGD(TAG, "Single click on page %d (no action)", current_page);
            }
            break;

        case BUTTON_ACTION_LONG:
            // 롱 프레스: 추후 설정 페이지 진입 등
            T_LOGI(TAG, "Long press (future: settings page)");
            break;

        case BUTTON_ACTION_LONG_RELEASE:
            // 롱 프레스 해제
            T_LOGD(TAG, "Long press release");
            break;

        default:
            break;
    }
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t button_handler_init(void)
{
    T_LOGI(TAG, "버튼 핸들러 초기화");
    return ESP_OK;
}

esp_err_t button_handler_start(void)
{
    if (s_started) {
        T_LOGW(TAG, "이미 시작됨");
        return ESP_OK;
    }

    // button_poll에 콜백 등록
    button_poll_set_callback(button_callback);

    s_started = true;
    T_LOGI(TAG, "버튼 핸들러 시작 (콜백 등록 완료)");
    return ESP_OK;
}

void button_handler_stop(void)
{
    if (!s_started) {
        return;
    }

    // 콜백 해제
    button_poll_set_callback(NULL);

    s_started = false;
    T_LOGI(TAG, "버튼 핸들러 정지");
}

void button_handler_deinit(void)
{
    button_handler_stop();
    T_LOGI(TAG, "버튼 핸들러 해제 완료");
}
