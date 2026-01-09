/**
 * @file prod_rx_app.cpp
 * @brief 프로덕션 Tally 수신 앱 구현
 */

#include "prod_rx_app.h"
#include "t_log.h"
#include "NVSConfig.h"
#include "nvs_flash.h"
#include "event_bus.h"
#include "config_service.h"
#include "hardware_service.h"
#include "DisplayManager.h"
#include "button_service.h"
#include "lora_service.h"
#include "device_manager.h"
#include "led_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <cstring>

// =============================================================================
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

static const char* TAG = "prod_rx_app";

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
        T_LOGI(TAG, "Camera ID 타이머 시작");
    }
}

static void stop_camera_id_timer(void)
{
    if (s_camera_id_timer != NULL) {
        xTimerStop(s_camera_id_timer, 0);
        T_LOGI(TAG, "Camera ID 타이머 정지");
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
        T_LOGI(TAG, "Camera ID 팝업 닫기 (클릭)");
        return ESP_OK;
    }

    // RxPage: 1 → 2 → 3 → 1 순환
    uint8_t current = display_manager_get_page_index();
    uint8_t next = (current == 1) ? 2 : (current == 2) ? 3 : 1;
    display_manager_switch_page(next);
    display_manager_force_refresh();
    T_LOGI(TAG, "RxPage: %d -> %d", current, next);

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
        T_LOGI(TAG, "Camera ID 팝업 표시 (롱프레스, max: %d)", max_camera);
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
                T_LOGI(TAG, "Camera ID 저장: %d -> %d (확인: %d)", old_id, new_id, saved_id);
            } else {
                T_LOGE(TAG, "Camera ID 저장 실패: %s", esp_err_to_name(ret));
            }
            // LED 업데이트는 led_service에서 EVT_CAMERA_ID_CHANGED로 처리
        } else {
            T_LOGI(TAG, "Camera ID 변경 없음: %d", new_id);
        }

        display_manager_set_camera_id_changing(false);
        display_manager_hide_camera_id_popup();
        display_manager_force_refresh();
        T_LOGI(TAG, "Camera ID 팝업 닫기 (롱프레스 해제)");
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

    // HardwareService 초기화
    ret = hardware_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "HardwareService init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // LoRa 초기화 (NVS에 저장된 RF 설정 사용)
    config_device_t device_config;
    // 기본값 (NVSConfig.h)
    float saved_freq = NVS_LORA_DEFAULT_FREQ;
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
        T_LOGI(TAG, "RF 설정 로드: %.1f MHz, Sync 0x%02X, SF%d, CR%d, BW%.0f, TXP%ddBm",
                 saved_freq, saved_sync, saved_sf, saved_cr, saved_bw, saved_txp);
    } else {
        T_LOGW(TAG, "RF 설정 로드 실패, 기본값 사용");
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
        T_LOGE(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
        return false;
    }
    T_LOGI(TAG, "LoRa 초기화 완료 (이벤트 기반 설정)");

    // WS2812 LED 초기화 (기본 색상으로)
    uint8_t camera_id = config_service_get_camera_id();
    esp_err_t led_ret = led_service_init_with_colors(-1, 0, camera_id, nullptr);
    if (led_ret == ESP_OK) {
        T_LOGI(TAG, "WS2812 초기화 완료 (카메라 ID: %d)", camera_id);
    } else {
        T_LOGW(TAG, "WS2812 초기화 실패: %s", esp_err_to_name(led_ret));
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
    T_LOGI(TAG, "  주파수: %.1f MHz", lora_config.frequency);
    T_LOGI(TAG, "  SF: %d, CR: 4/%d, BW: %.0f kHz",
             lora_config.spreading_factor,
             lora_config.coding_rate,
             lora_config.bandwidth);
    T_LOGI(TAG, "  전력: %d dBm, SyncWord: 0x%02X",
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
    T_LOGI(TAG, "HardwareService 시작");

    // LoRa 시작
    lora_service_start();
    T_LOGI(TAG, "LoRa 시작");

    // DeviceManager 시작 (상태 요청 수신 처리)
    device_manager_start();
    T_LOGI(TAG, "DeviceManager 시작");

    // DisplayManager 시작 (이벤트 구독 먼저 완료)
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 저장된 설정 로드 및 이벤트 발행 (DisplayManager 구독 완료 후)
    config_all_t saved_config;
    esp_err_t ret = config_service_load_all(&saved_config);
    if (ret == ESP_OK) {
        // 카메라 ID 이벤트 발행 (DisplayManager가 구독 완료된 상태)
        event_bus_publish(EVT_CAMERA_ID_CHANGED, &saved_config.device.camera_id, sizeof(uint8_t));
        T_LOGI(TAG, "카메라 ID 이벤트 발행: %d", saved_config.device.camera_id);
        // 밝기 이벤트 발행
        event_bus_publish(EVT_BRIGHTNESS_CHANGED, &saved_config.device.brightness, sizeof(uint8_t));
        T_LOGI(TAG, "밝기 이벤트 발행: %d", saved_config.device.brightness);
        // RF 설정 이벤트 발행 (DisplayManager용, 드라이버는 init에서 이미 설정됨)
        lora_rf_event_t rf_event = {
            .frequency = saved_config.device.rf.frequency,
            .sync_word = saved_config.device.rf.sync_word
        };
        event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
        T_LOGI(TAG, "RF 설정 이벤트 발행 (디스플레이용): %.1f MHz, Sync 0x%02X",
                 rf_event.frequency, rf_event.sync_word);
    } else {
        T_LOGW(TAG, "설정 로드 실패: %s", esp_err_to_name(ret));
    }

#ifdef DEVICE_MODE_RX
    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);
    T_LOGI(TAG, "버튼 이벤트 구독 시작");
#endif

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

    // RX 페이지로 전환
    display_manager_boot_complete();

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
