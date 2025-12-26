/**
 * @file button_handler.c
 * @brief 버튼 핸들러 구현
 *
 * button_poll 서비스의 이벤트를 받아서
 * event_bus로 이벤트를 발행합니다.
 */

#include "button_handler.h"
#include "button_poll.h"
#include "event_bus.h"
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
    switch (action) {
        case BUTTON_ACTION_SINGLE:
            // 단일 클릭 이벤트 발행
            event_bus_publish(EVT_BUTTON_SINGLE_CLICK, NULL, 0);
            T_LOGD(TAG, "Single click event published");
            break;

        case BUTTON_ACTION_LONG:
            // 롱 프레스 이벤트 발행
            event_bus_publish(EVT_BUTTON_LONG_PRESS, NULL, 0);
            T_LOGD(TAG, "Long press event published");
            break;

        case BUTTON_ACTION_LONG_RELEASE:
            // 롱 프레스 해제 이벤트 발행
            event_bus_publish(EVT_BUTTON_LONG_RELEASE, NULL, 0);
            T_LOGD(TAG, "Long release event published");
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
    T_LOGI(TAG, "버튼 핸들러 시작");
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
