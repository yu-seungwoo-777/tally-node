/**
 * @file button_service.c
 * @brief 버튼 서비스 구현
 */

#include "button_service.h"
#include "button_poll.h"
#include "esp_log.h"

static const char* TAG = "BUTTON_SERVICE";

static button_callback_t s_user_callback = NULL;

/**
 * @brief 내부 버튼 이벤트 핸들러
 */
static void on_button_event(button_action_t action)
{
    if (s_user_callback != NULL) {
        s_user_callback(action);
    }
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t button_service_init(void)
{
    ESP_LOGI(TAG, "버튼 서비스 초기화");
    esp_err_t ret = button_poll_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "버튼 폴링 초기화 실패");
        return ret;
    }
    ESP_LOGI(TAG, "버튼 서비스 초기화 완료");
    return ESP_OK;
}

esp_err_t button_service_start(void)
{
    ESP_LOGI(TAG, "버튼 서비스 시작");
    esp_err_t ret = button_poll_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "버튼 폴링 시작 실패");
        return ret;
    }

    // 내부 콜백 설정
    button_poll_set_callback(on_button_event);

    ESP_LOGI(TAG, "버튼 서비스 시작 완료");
    return ESP_OK;
}

void button_service_stop(void)
{
    ESP_LOGI(TAG, "버튼 서비스 중지");
    button_poll_stop();
}

void button_service_deinit(void)
{
    ESP_LOGI(TAG, "버튼 서비스 해제");
    button_poll_deinit();
}

void button_service_set_callback(button_callback_t callback)
{
    s_user_callback = callback;
}

bool button_service_is_pressed(void)
{
    return button_poll_is_pressed();
}
