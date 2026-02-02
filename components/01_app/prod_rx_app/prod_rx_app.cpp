/**
 * @file prod_rx_app.cpp
 * @brief 프로덕션 Tally 수신 앱 구현
 */

#include "prod_rx_app.h"
#include "t_log.h"
#include "NVSConfig.h"
#include "event_bus.h"
#include "config_service.h"
#include "hardware_service.h"
#include "DisplayManager.h"
#include "button_service.h"
#include "lora_service.h"
#include "lora_driver.h"
#include "device_manager.h"
#include "led_service.h"
#include "battery_driver.h"
#include "BatteryEmptyPage.h"
#include "RxPage.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <cstring>

static const char* TAG = "01_RxApp";

// ============================================================================
// 배터리 Empty 타이머 (공통)
// ============================================================================

// 배터리 empty 상태에서 10초 후 딥슬립 진입
static TimerHandle_t s_battery_empty_timer = NULL;
static uint8_t s_deep_sleep_countdown = 0;  // 카운트다운 값 (초)

static void battery_empty_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (s_deep_sleep_countdown > 0) {
        s_deep_sleep_countdown--;
        display_manager_set_deep_sleep_countdown(s_deep_sleep_countdown);
        display_manager_force_refresh();

        if (s_deep_sleep_countdown == 0) {
            // 카운트다운 종료, 전압 표시 후 딥슬립 진입
            T_LOGW(TAG, "Battery empty - Showing voltage, then deep sleep");
            battery_empty_page_set_timer_completed(true);
            display_manager_set_deep_sleep_countdown(0);
            display_manager_force_refresh();

            // 짧은 지연 후 딥슬립 진입 (전압 표시 확인용)
            vTaskDelay(pdMS_TO_TICKS(2000));
            xTimerStop(s_battery_empty_timer, 0);
            esp_deep_sleep_start();
        } else {
            T_LOGD(TAG, "Deep sleep countdown: %d", s_deep_sleep_countdown);
        }
    }
}

static void start_battery_empty_timer(void)
{
    if (s_battery_empty_timer == NULL) {
        s_battery_empty_timer = xTimerCreate(
            "batt_empty_timer",
            pdMS_TO_TICKS(1000),   // 1초 간격
            pdTRUE,                // 반복
            NULL,
            battery_empty_timer_callback
        );
    }
    if (s_battery_empty_timer != NULL && xTimerStart(s_battery_empty_timer, 0) == pdPASS) {
        s_deep_sleep_countdown = 10;  // 10초 카운트다운 시작
        display_manager_set_deep_sleep_countdown(s_deep_sleep_countdown);
        T_LOGW(TAG, "Battery empty timer started - Deep sleep in 10 seconds");
    }
}

/**
 * @brief 배터리 엠티 체크 (1초마다 EVT_INFO_UPDATED로 호출)
 */
static void check_battery_empty(void)
{
    // 이미 배터리 엠티 상태면 타이머가 실행 중이므로 체크 생략
    if (s_battery_empty_timer != NULL && xTimerIsTimerActive(s_battery_empty_timer)) {
        return;
    }

    // 배터리 상태 체크
    battery_status_t status;
    if (battery_driver_update_status(&status) == ESP_OK) {
        if (status.voltage < 3.2f) {
            T_LOGW(TAG, "Battery empty detected (%.2fV < 3.2V) - Showing empty page, deep sleep in 10s", status.voltage);
            display_manager_set_battery_empty(true);
            start_battery_empty_timer();
        }
    }
}

// ============================================================================
// 카메라 ID 변경 타이머 (RX 전용)
// ============================================================================

#ifdef DEVICE_MODE_RX
static TimerHandle_t s_camera_id_timer = NULL;

static void camera_id_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    // RX 페이지에서 카메라 ID 팝업 상태(1)인 경우에만 처리
    if (display_manager_get_state() == 1) {
        if (display_manager_is_camera_id_changing()) {
            uint8_t max_camera = config_service_get_max_camera_num();
            display_manager_cycle_camera_id(max_camera);
            display_manager_force_refresh();
        }
    }
}

static void start_camera_id_timer(void)
{
    if (s_camera_id_timer == NULL) {
        s_camera_id_timer = xTimerCreate(
            "cam_id_timer",
            pdMS_TO_TICKS(800),   // 0.8초 주기
            pdTRUE,               // 반복
            NULL,
            camera_id_timer_callback
        );
    }
    if (s_camera_id_timer != NULL) {
        xTimerStart(s_camera_id_timer, 0);
        T_LOGD(TAG, "Camera ID timer started");
    }
}

static void stop_camera_id_timer(void)
{
    if (s_camera_id_timer != NULL) {
        xTimerStop(s_camera_id_timer, 0);
        T_LOGD(TAG, "Camera ID timer stopped");
    }
}
#endif // DEVICE_MODE_RX

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

#ifdef DEVICE_MODE_RX
static esp_err_t handle_button_single_click(const event_data_t* event)
{
    (void)event;

    // 카메라 ID 팝업 상태(1)면 닫기
    if (display_manager_get_state() == 1) {
        display_manager_hide_camera_id_popup();
        stop_camera_id_timer();
        display_manager_force_refresh();
        T_LOGD(TAG, "Camera ID popup closed (click)");
        return ESP_OK;
    }

    // RxPage 순환 (1 -> 2 -> ... -> RX_PAGE_COUNT -> 1)
    uint8_t current = display_manager_get_page_index();
    uint8_t page_count = rx_page_get_page_count();
    uint8_t next = (current >= page_count) ? 1 : (current + 1);
    display_manager_switch_page(next);
    display_manager_force_refresh();
    T_LOGD(TAG, "RxPage: %d -> %d", current, next);

    return ESP_OK;
}

static esp_err_t handle_button_long_press(const event_data_t* event)
{
    (void)event;

    // 롱 프레스: 카메라 ID 변경 팝업 표시
    if (display_manager_get_state() == 0) {
        uint8_t max_camera = config_service_get_max_camera_num();
        display_manager_show_camera_id_popup(max_camera);
        display_manager_set_camera_id_changing(true);
        start_camera_id_timer();
        display_manager_force_refresh();
        T_LOGD(TAG, "Camera ID popup shown (long press, max: %d)", max_camera);
    }

    return ESP_OK;
}

static esp_err_t handle_button_long_release(const event_data_t* event)
{
    (void)event;

    // 롱 프레스 해제: 카메라 ID 저장
    if (display_manager_get_state() == 1) {
        stop_camera_id_timer();

        uint8_t new_id = display_manager_get_display_camera_id();
        uint8_t old_id = config_service_get_camera_id();

        if (new_id != old_id) {
            esp_err_t ret = config_service_set_camera_id(new_id);
            if (ret == ESP_OK) {
                uint8_t saved_id = config_service_get_camera_id();
                T_LOGI(TAG, "Camera ID saved: %d -> %d (verified: %d)", old_id, new_id, saved_id);
            } else {
                T_LOGE(TAG, "Camera ID save failed: %s", esp_err_to_name(ret));
            }
            // LED 업데이트는 led_service에서 EVT_CAMERA_ID_CHANGED로 처리
        } else {
            T_LOGI(TAG, "Camera ID unchanged: %d", new_id);
        }

        display_manager_set_camera_id_changing(false);
        display_manager_hide_camera_id_popup();
        display_manager_force_refresh();
        T_LOGD(TAG, "Camera ID popup closed (long press released)");
    }

    return ESP_OK;
}
#endif // DEVICE_MODE_RX

bool prod_rx_app_init(const prod_rx_config_t* config)
{
    (void)config;

    if (s_app.initialized) {
        T_LOGW(TAG, "Already initialized");
        return true;
    }

    T_LOGI(TAG, "RX app init...");

    esp_err_t ret;

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

    // HardwareService 초기화
    ret = hardware_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "HardwareService init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // LoRa 초기화 (NVS에 저장된 RF 설정 사용)
    config_device_t device_config;
    // 칩 감지 후 기본값 결정
    lora_chip_type_t chip = lora_driver_detect_chip();
    float default_freq = (chip == LORA_CHIP_SX1268_433M) ? NVS_LORA_DEFAULT_FREQ_433 : NVS_LORA_DEFAULT_FREQ_868;

    float saved_freq = default_freq;
    uint8_t saved_sync = NVS_LORA_DEFAULT_SYNC_WORD;
    uint8_t saved_sf = NVS_LORA_DEFAULT_SF;
    uint8_t saved_cr = NVS_LORA_DEFAULT_CR;
    float saved_bw = NVS_LORA_DEFAULT_BW;
    int8_t saved_txp = NVS_LORA_DEFAULT_TX_POWER;

    if (config_service_get_device(&device_config) == ESP_OK) {
        saved_freq = device_config.rf.frequency;
        saved_sync = device_config.rf.sync_word;
        saved_sf = device_config.rf.sf;
        saved_cr = device_config.rf.cr;
        saved_bw = device_config.rf.bw;
        saved_txp = device_config.rf.tx_power;
        T_LOGD(TAG, "RF config loaded: %.1f MHz, Sync 0x%02X, SF%d, CR%d, BW%.0f, TXP%ddBm",
                 saved_freq, saved_sync, saved_sf, saved_cr, saved_bw, saved_txp);
    } else {
        T_LOGI(TAG, "RF config using chip defaults: %.1f MHz", default_freq);
    }

    lora_service_config_t lora_config = {
        .frequency = saved_freq,
        .spreading_factor = saved_sf,
        .coding_rate = saved_cr,
        .bandwidth = saved_bw,
        .tx_power = saved_txp,
        .sync_word = saved_sync
    };
    esp_err_t lora_ret = lora_service_init(&lora_config);
    if (lora_ret != ESP_OK) {
        T_LOGE(TAG, "LoRa init failed: %s", esp_err_to_name(lora_ret));
        return false;
    }
    T_LOGI(TAG, "LoRa init complete (event-based config)");

    // WS2812 LED 초기화 (기본 색상으로)
    uint8_t camera_id = config_service_get_camera_id();
    esp_err_t led_ret = led_service_init_with_colors(-1, 0, camera_id, nullptr);
    if (led_ret == ESP_OK) {
        T_LOGI(TAG, "WS2812 init complete (camera ID: %d)", camera_id);
    } else {
        T_LOGW(TAG, "WS2812 init failed: %s", esp_err_to_name(led_ret));
    }

    // DisplayManager 초기화 (RxPage 자동 등록됨)
    if (!display_manager_init()) {
        T_LOGE(TAG, "DisplayManager init failed");
        return false;
    }

    // 버튼 서비스 초기화
    ret = button_service_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Button service init failed: %s", esp_err_to_name(ret));
    }

    // DeviceManager 초기화 (이벤트 구독)
    ret = device_manager_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "DeviceManager init failed: %s", esp_err_to_name(ret));
    }

    s_app.initialized = true;
    T_LOGI(TAG, "RX app init complete");

    // LoRa 설정 로그
    T_LOGD(TAG, "  Frequency: %.1f MHz", lora_config.frequency);
    T_LOGD(TAG, "  SF: %d, CR: 4/%d, BW: %.0f kHz",
             lora_config.spreading_factor,
             lora_config.coding_rate,
             lora_config.bandwidth);
    T_LOGD(TAG, "  Power: %d dBm, SyncWord: 0x%02X",
             lora_config.tx_power,
             lora_config.sync_word);

    return true;
}

void prod_rx_app_start(void)
{
    if (!s_app.initialized) {
        T_LOGE(TAG, "Not initialized");
        return;
    }

    if (s_app.running) {
        T_LOGW(TAG, "Already running");
        return;
    }

    // HardwareService 시작 (모니터링 태스크)
    hardware_service_start();
    T_LOGI(TAG, "HardwareService started");

    // LoRa 시작
    lora_service_start();
    T_LOGI(TAG, "LoRa started");

    // DeviceManager 시작 (상태 요청 수신 처리)
    device_manager_start();
    T_LOGI(TAG, "DeviceManager started");

    // DisplayManager 시작 (이벤트 구독 먼저 완료)
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 저장된 설정 로드 및 이벤트 발행 (DisplayManager 구독 완료 후)
    config_all_t saved_config;
    esp_err_t ret = config_service_load_all(&saved_config);
    if (ret == ESP_OK) {
        // 카메라 ID 이벤트 발행
        event_bus_publish(EVT_CAMERA_ID_CHANGED, &saved_config.device.camera_id, sizeof(uint8_t));
        T_LOGD(TAG, "Camera ID event published: %d", saved_config.device.camera_id);
        // 밝기 이벤트 발행
        event_bus_publish(EVT_BRIGHTNESS_CHANGED, &saved_config.device.brightness, sizeof(uint8_t));
        T_LOGD(TAG, "Brightness event published: %d", saved_config.device.brightness);
        // LED 색상 요청 이벤트 발행 (NVS에서 로드)
        event_bus_publish(EVT_LED_COLORS_REQUEST, NULL, 0);
        T_LOGD(TAG, "LED colors request event published");
        // RF 설정 이벤트 발행
        lora_rf_event_t rf_event = {
            .frequency = saved_config.device.rf.frequency,
            .sync_word = saved_config.device.rf.sync_word
        };
        event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
        T_LOGD(TAG, "RF config event published: %.1f MHz, Sync 0x%02X",
                 rf_event.frequency, rf_event.sync_word);
    } else {
        T_LOGW(TAG, "Config load failed: %s", esp_err_to_name(ret));
    }

#ifdef DEVICE_MODE_RX
    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);
    T_LOGD(TAG, "Button event subscription completed");
#endif

    // 배터리 엠티 체크 (1초마다 HardwareService에서 EVT_INFO_UPDATED 발행)
    event_bus_subscribe(EVT_INFO_UPDATED, [](const event_data_t* event) -> esp_err_t {
        (void)event;
        check_battery_empty();
        return ESP_OK;
    });
    T_LOGD(TAG, "Battery empty check subscription completed");

    // 버튼 서비스 시작
    button_service_start();

    // 부팅 시나리오
    const char* boot_messages[] = {
        "Init NVS",
        "Init EventBus",
        "Init Config",
        "Init LoRa",
        "RX Ready"
    };

    for (int i = 0; i < 5; i++) {
        int progress = (i + 1) * 20;
        display_manager_boot_set_message(boot_messages[i]);
        display_manager_boot_set_progress(progress);
        display_manager_force_refresh();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 부팅 시 배터리 체크 (app 레이어에서 직접 확인)
    // 상태응답 → 배터리 체크 → 배터리 empty 페이지 → 10초 후 딥슬립
    battery_status_t battery_status = { .voltage = 4.0f, .percent = 100 };  // 기본값
    bool battery_check_ok = false;

    if (battery_driver_update_status(&battery_status) == ESP_OK) {
        battery_check_ok = true;
        T_LOGI(TAG, "Boot battery check: %d%% (%.2fV)", battery_status.percent, battery_status.voltage);
        if (battery_status.voltage < 3.2f) {
            T_LOGW(TAG, "Battery empty (%.2fV < 3.2V) - Showing empty page, deep sleep in 10s", battery_status.voltage);
            display_manager_set_battery_empty(true);
            start_battery_empty_timer();
        }
    } else {
        T_LOGW(TAG, "Battery status read failed at boot - assuming normal");
    }

    // 배터리 정상이면 RX 페이지로 전환
    if (!battery_check_ok || battery_status.voltage >= 3.2f) {
        display_manager_boot_complete();
    }

    // 카메라 ID는 EVT_CAMERA_ID_CHANGED 이벤트로 DisplayManager에 전달됨

    s_app.running = true;
    T_LOGI(TAG, "RX app started");
}

void prod_rx_app_stop(void)
{
    if (!s_app.running) {
        return;
    }

    // DeviceManager 중지
    device_manager_stop();

#ifdef DEVICE_MODE_RX
    // 버튼 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_unsubscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_unsubscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);
#endif

    button_service_stop();

    // LoRa 정지
    lora_service_stop();

#ifdef DEVICE_MODE_RX
    stop_camera_id_timer();
#endif

    s_app.running = false;
    T_LOGI(TAG, "RX app stopped");
}

void prod_rx_app_deinit(void)
{
    prod_rx_app_stop();

    button_service_deinit();

    // WS2812 정리
    led_service_deinit();

    // LoRa 정리
    lora_service_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "RX app deinit complete");
}

void prod_rx_app_loop(void)
{
    if (!s_app.running) {
        return;
    }

    // 디스플레이 갱신 (500ms 주기, DisplayManager 내부에서 체크)
    display_manager_update();

    // System 데이터는 HardwareService가 EVT_INFO_UPDATED로 발행 (1초마다)
    // DisplayManager가 이벤트를 구독하여 자동 갱신됨
}

void prod_rx_app_print_status(void)
{
    T_LOGI(TAG, "===== RX App Status =====");
    T_LOGI(TAG, "Running: %s", s_app.running ? "Yes" : "No");

    T_LOGI(TAG, "=========================");
}

bool prod_rx_app_is_running(void)
{
    return s_app.running;
}

} // extern "C"
