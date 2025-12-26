/**
 * @file prod_tx_app.cpp
 * @brief 프로덕션 Tally 송신 앱 구현
 */

#include "prod_tx_app.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "event_bus.h"
#include "ConfigService.h"
#include "DisplayManager.h"
#include "button_poll.h"
#include "button_handler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// NVS 초기화 헬퍼
// ============================================================================

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            return ret;
        }
        ret = nvs_flash_init();
    }

    return ret;
}

static const char* TAG = "prod_tx_app";

// ============================================================================
// 버튼 이벤트 핸들러
// ============================================================================

static esp_err_t handle_button_single_click(const event_data_t* event)
{
    (void)event;

    // TxPage: 1 -> 2 -> 3 -> 4 -> 5 -> 1 순환
    uint8_t current = display_manager_get_page_index();
    uint8_t next = (current == 5) ? 1 : (current + 1);
    display_manager_switch_page(next);
    display_manager_force_refresh();
    T_LOGI(TAG, "TxPage: %d -> %d", current, next);

    return ESP_OK;
}

static esp_err_t handle_button_long_press(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "Long press (TX mode: no action)");
    return ESP_OK;
}

static esp_err_t handle_button_long_release(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Long press release");
    return ESP_OK;
}

// ============================================================================
// 앱 상태
// ============================================================================

static struct {
    bool running;
    bool initialized;
} s_app = {
    .running = false,
    .initialized = false
};

extern "C" {

bool prod_tx_app_init(const prod_tx_config_t* config) {
    (void)config;

    if (s_app.initialized) {
        T_LOGW(TAG, "Already initialized");
        return true;
    }

    T_LOGI(TAG, "TX app init...");

    // NVS 초기화
    esp_err_t ret = init_nvs();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // event_bus 초기화
    ret = event_bus_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "EventBus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // ConfigService 초기화
    ret = config_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "ConfigService init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // DisplayManager 초기화 (TxPage 자동 등록됨)
    if (!display_manager_init()) {
        T_LOGE(TAG, "DisplayManager init failed");
        return false;
    }

    // 버튼 폴링 초기화
    ret = button_poll_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Button poll init failed: %s", esp_err_to_name(ret));
    }

    // 버튼 핸들러 초기화
    ret = button_handler_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Button handler init failed: %s", esp_err_to_name(ret));
    }

    s_app.initialized = true;
    T_LOGI(TAG, "TX app init complete");
    return true;
}

void prod_tx_app_start(void) {
    if (!s_app.initialized) {
        T_LOGE(TAG, "Not initialized");
        return;
    }

    if (s_app.running) {
        T_LOGW(TAG, "Already running");
        return;
    }

    // DisplayManager 시작, BootPage로 전환
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

    // 버튼 폴링 시작
    button_poll_start();

    // 버튼 핸들러 시작
    button_handler_start();

    // 부팅 시나리오
    const char* boot_messages[] = {
        "Init NVS",
        "Init EventBus",
        "Init Config",
        "Init LoRa",
        "TX Ready"
    };

    for (int i = 0; i < 5; i++) {
        int progress = (i + 1) * 20;
        display_manager_boot_set_message(boot_messages[i]);
        display_manager_boot_set_progress(progress);
        display_manager_force_refresh();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // TX 페이지로 전환
    display_manager_boot_complete();

    s_app.running = true;
    T_LOGI(TAG, "TX app started");
}

void prod_tx_app_stop(void) {
    if (!s_app.running) {
        return;
    }

    // 버튼 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_unsubscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_unsubscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

    button_handler_stop();
    button_poll_stop();

    s_app.running = false;
    T_LOGI(TAG, "TX app stopped");
}

void prod_tx_app_deinit(void) {
    prod_tx_app_stop();

    button_handler_deinit();
    button_poll_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "TX app deinit complete");
}

void prod_tx_app_loop(void) {
    if (!s_app.running) {
        return;
    }

    // 디스플레이 갱신 (200ms 주기, DisplayManager 내부에서 체크)
    display_manager_update();

    // System 데이터 1000ms마다 갱신
    static uint32_t last_sys_update = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_sys_update >= 1000) {
        last_sys_update = now;

        // ConfigService에서 System 데이터 읽기
        config_system_t sys;
        config_service_get_system(&sys);
        config_service_update_battery();  // ADC 읽기 (내부에서 battery 갱신)
        config_service_get_system(&sys);  // 갱신된 battery 다시 읽기

        // DisplayManager에 개별 파라미터 전달
        display_manager_update_system(sys.device_id, sys.battery,
                                      config_service_get_voltage(),
                                      config_service_get_temperature());
    }
}

void prod_tx_app_print_status(void) {
    T_LOGI(TAG, "===== TX App Status =====");
    T_LOGI(TAG, "Running: %s", s_app.running ? "Yes" : "No");
    T_LOGI(TAG, "=========================");
}

bool prod_tx_app_is_running(void) {
    return s_app.running;
}

} // extern "C"
