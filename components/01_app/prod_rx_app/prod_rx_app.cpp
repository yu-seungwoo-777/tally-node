/**
 * @file prod_rx_app.cpp
 * @brief 프로덕션 Tally 수신 앱 구현
 */

#include "prod_rx_app.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "event_bus.h"
#include "config_service.h"
#include "hardware_service.h"
#include "DisplayManager.h"
#include "button_service.h"
#include "lora_service.h"
#include "device_manager.h"
#include "led_service.h"
#include "ws2812_driver.h"
#include "TallyTypes.h"

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
    bool stopped;         // 기능 정지 상태
    // 현재 Tally 상태 (카메라 ID 변경 시 LED 업데이트용)
    bool program_active;   // 현재 카메라가 PGM인지
    bool preview_active;   // 현재 카메라가 PVW인지
} s_app = {
    .running = false,
    .initialized = false,
    .stopped = false,
    .program_active = false,
    .preview_active = false
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

    // RxPage: 1 ↔ 2 토글
    uint8_t current = display_manager_get_page_index();
    uint8_t next = (current == 1) ? 2 : 1;
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

                // WS2812에도 새 카메라 ID 적용
                led_service_set_camera_id(new_id);

                // 현재 Tally 상태에 따라 LED 즉시 업데이트
                // (새 패킷을 기다리지 않고 현재 상태 적용)
                if (s_app.program_active) {
                    led_service_set_state(WS2812_PROGRAM);
                    T_LOGI(TAG, "LED 즉시 업데이트: PROGRAM");
                } else if (s_app.preview_active) {
                    led_service_set_state(WS2812_PREVIEW);
                    T_LOGI(TAG, "LED 즉시 업데이트: PREVIEW");
                } else {
                    led_service_set_state(WS2812_OFF);
                    T_LOGI(TAG, "LED 즉시 업데이트: OFF");
                }
            } else {
                T_LOGE(TAG, "Camera ID 저장 실패: %s", esp_err_to_name(ret));
            }
            display_manager_set_cam_id(new_id);
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

/**
 * @brief Tally 상태 변경 이벤트 핸들러
 * 현재 상태를 저장하여 카메라 ID 변경 시 LED 즉시 업데이트에 사용
 */
static esp_err_t handle_tally_state_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 기능 정지 상태에서는 Tally 무시
    if (s_app.stopped) {
        return ESP_OK;
    }

    const tally_event_data_t* tally_evt = (const tally_event_data_t*)event->data;

    // packed_data_t 구조체 생성
    packed_data_t tally = {
        .data = (uint8_t*)tally_evt->tally_data,
        .data_size = static_cast<uint8_t>((tally_evt->channel_count + 3) / 4),
        .channel_count = tally_evt->channel_count
    };

    // 현재 카메라 ID
    uint8_t my_camera_id = config_service_get_camera_id();

    // 내 카메라의 Tally 상태 확인 (채널 번호는 1-based)
    if (my_camera_id > 0 && my_camera_id <= tally.channel_count) {
        uint8_t status = packed_data_get_channel(&tally, my_camera_id);
        s_app.program_active = (status == TALLY_STATUS_PROGRAM || status == TALLY_STATUS_BOTH);
        s_app.preview_active = (status == TALLY_STATUS_PREVIEW || status == TALLY_STATUS_BOTH);
    } else {
        s_app.program_active = false;
        s_app.preview_active = false;
    }

    return ESP_OK;
}

/**
 * @brief 기능 정지 상태 변경 이벤트 핸들러
 */
static esp_err_t handle_stop_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool* stopped = (const bool*)event->data;
    s_app.stopped = *stopped;

    if (s_app.stopped) {
        T_LOGW(TAG, "기능 정지: LED OFF");
        // LED 전체 소등
        led_service_off();
    } else {
        T_LOGI(TAG, "기능 정지 해제");
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

    // LoRa 초기화 (기본값으로, EVT_RF_CHANGED에서 설정 업데이트)
    lora_service_config_t lora_config = {
        .frequency = 868.0f,  // 기본값
        .spreading_factor = 7,
        .coding_rate = 5,
        .bandwidth = 250.0f,
        .tx_power = 22,
        .sync_word = 0x12
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

    // 저장된 설정 로드 및 이벤트 발행 (device_manager가 구독 완료 후)
    config_all_t saved_config;
    esp_err_t ret = config_service_load_all(&saved_config);
    if (ret == ESP_OK) {
        // 카메라 ID 이벤트 발행
        event_bus_publish(EVT_CAMERA_ID_CHANGED, &saved_config.device.camera_id, sizeof(uint8_t));
        T_LOGI(TAG, "카메라 ID 이벤트 발행: %d", saved_config.device.camera_id);
        // 밝기 이벤트 발행
        event_bus_publish(EVT_BRIGHTNESS_CHANGED, &saved_config.device.brightness, sizeof(uint8_t));
        T_LOGI(TAG, "밝기 이벤트 발행: %d", saved_config.device.brightness);
    } else {
        T_LOGW(TAG, "설정 로드 실패: %s", esp_err_to_name(ret));
    }

    // DisplayManager 시작, BootPage로 전환
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

#ifdef DEVICE_MODE_RX
    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

    // Tally 상태 변경 이벤트 구독 (카메라 ID 변경 시 LED 즉시 업데이트용)
    event_bus_subscribe(EVT_TALLY_STATE_CHANGED, handle_tally_state_changed);
    event_bus_subscribe(EVT_STOP_CHANGED, handle_stop_changed);
    T_LOGI(TAG, "Tally 상태 이벤트 구독 시작");
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

#ifdef DEVICE_MODE_RX
    // ConfigService에서 카메라 ID 로드하여 RxPage에 설정
    uint8_t saved_camera_id = config_service_get_camera_id();
    display_manager_set_cam_id(saved_camera_id);
    T_LOGI(TAG, "카메라 ID 로드: %d", saved_camera_id);
#endif

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
    event_bus_unsubscribe(EVT_TALLY_STATE_CHANGED, handle_tally_state_changed);
    event_bus_unsubscribe(EVT_STOP_CHANGED, handle_stop_changed);
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
