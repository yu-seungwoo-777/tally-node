/**
 * @file display_test_app.cpp
 * @brief 디스플레이 테스트 앱 구현
 *
 * BootPage를 표시하고 진행률을 업데이트하는 테스트 앱
 */

#include "display_test_app.h"
#include "DisplayManager.h"
#include "t_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "DisplayTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t display_test_app_init(void)
{
    T_LOGI(TAG, "========================================");
    T_LOGI(TAG, "디스플레이 테스트 앱 초기화");
    T_LOGI(TAG, "========================================");

    // DisplayManager 초기화 (내부에서 BootPage 자동 초기화)
    T_LOGI(TAG, "DisplayManager 초기화 중...");
    if (!display_manager_init()) {
        T_LOGE(TAG, "DisplayManager 초기화 실패");
        return ESP_FAIL;
    }
    T_LOGI(TAG, "DisplayManager 초기화 완료");

    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 초기화 완료");
    return ESP_OK;
}

esp_err_t display_test_app_start(void)
{
    if (s_running) {
        T_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    T_LOGI(TAG, "디스플레이 테스트 앱 시작 중...");

    // DisplayManager 시작
    display_manager_start();

    // BootPage로 전환
    display_manager_set_page(PAGE_BOOT);

    s_running = true;
    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 시작 완료");

    // 부팅 시나리오 실행 (메시지 업데이트)
    T_LOGI(TAG, "");
    T_LOGI(TAG, "===== 부팅 시나리오 시작 =====");

    const char* boot_messages[] = {
        "LoRa init...",
        "Network init...",
        "Loading config...",
        "Starting services...",
        "System ready"
    };

    for (int i = 0; i < 5; i++) {
        int progress = (i + 1) * 20;
        display_manager_boot_set_message(boot_messages[i]);
        display_manager_boot_set_progress(progress);
        T_LOGI(TAG, "[%d%%] %s", progress, boot_messages[i]);
        display_manager_force_refresh();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    T_LOGI(TAG, "===== 부팅 시나리오 완료 =====");
    T_LOGI(TAG, "");

    return ESP_OK;
}

void display_test_app_stop(void)
{
    if (!s_running) {
        return;
    }

    T_LOGI(TAG, "디스플레이 테스트 앱 정지 중...");
    s_running = false;
    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 정지 완료");
}

bool display_test_app_is_running(void)
{
    return s_running;
}

} // extern "C"
