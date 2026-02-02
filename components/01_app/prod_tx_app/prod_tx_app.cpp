/**
 * @file prod_tx_app.cpp
 * @brief 프로덕션 Tally 송신 앱 구현
 */

#include "prod_tx_app.h"
#include "PackedData.h"
#include "t_log.h"
#include "NVSConfig.h"
#include "event_bus.h"
#include "config_service.h"
#include "hardware_service.h"
#include "license_service.h"
#include "DisplayManager.h"
#include "BootPage.h"
#include "TxPage.h"
#include "button_service.h"
#include "switcher_service.h"
#include "network_service.h"
#include "lora_service.h"
#include "lora_driver.h"
#include "device_manager.h"
#include "web_server.h"
#include "tally_test_service.h"
#include "TallyTypes.h"
#include "battery_driver.h"
#include "app_types.h"
#include "BatteryEmptyPage.h"
#include "esp_sleep.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <cstring>

static const char* TAG = "01_TxApp";

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
 * @brief 배터리 엤티 체크 (1초마다 EVT_INFO_UPDATED로 호출)
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
// 버튼 이벤트 핸들러 (TX 전용)
// ============================================================================

#ifdef DEVICE_MODE_TX
static esp_err_t handle_button_single_click(const event_data_t* event)
{
    (void)event;

    // TxPage 순환 (1 -> 2 -> ... -> TX_PAGE_COUNT -> 1)
    uint8_t current = display_manager_get_page_index();
    uint8_t page_count = tx_page_get_page_count();
    uint8_t next = (current >= page_count) ? 1 : (current + 1);
    display_manager_switch_page(next);
    display_manager_force_refresh();
    T_LOGD(TAG, "TxPage: %d -> %d", current, next);

    return ESP_OK;
}

static esp_err_t handle_button_long_press(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "Long press -> License validation attempt");

    // 저장된 라이센스 키로 검증
    char license_key[17] = {0};
    if (license_service_get_key(license_key) == ESP_OK && strlen(license_key) == 16) {
        T_LOGI(TAG, "Validating with saved license key: %.16s", license_key);
        license_service_validate(license_key);
    } else {
        T_LOGW(TAG, "No license key, skipping validation");
    }
    return ESP_OK;
}

static esp_err_t handle_button_long_release(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Long press release");
    return ESP_OK;
}
#endif // DEVICE_MODE_TX

// ============================================================================
// 테스트 모드 이벤트 핸들러
// ============================================================================

static esp_err_t handle_test_mode_start(const event_data_t* event)
{
    if (event) {
        const tally_test_mode_config_t* config = (const tally_test_mode_config_t*)event->data;
        T_LOGI(TAG, "Test mode start: channels=%d, interval=%dms", config->max_channels, config->interval_ms);

        esp_err_t ret = tally_test_service_start(config->max_channels, config->interval_ms);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "Test mode start failed: %s", esp_err_to_name(ret));
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t handle_test_mode_stop(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "Test mode stopped");
    tally_test_service_stop();
    return ESP_OK;
}

// ============================================================================
// 앱 상태
// ============================================================================

static struct {
    switcher_service_handle_t service;
    bool running;
    bool initialized;
} s_app = {
    .service = nullptr,
    .running = false,
    .initialized = false
};

// ============================================================================
// SwitcherService 콜백 핸들러
// ============================================================================

// LoRa 송신은 SwitcherService에서 직접 처리하므로 콜백 불필요
// (라이선스 확인, 송신 로직이 Service 레이어로 이동됨)

static void on_connection_change(connection_state_t state)
{
    T_LOGD(TAG, "Connection state changed: %s", connection_state_to_string(state));
}

static void on_switcher_change(switcher_role_t role)
{
    T_LOGD(TAG, "%s switcher change detected", switcher_role_to_string(role));
}

// ============================================================================
// Tally 상태 변경 핸들러 (테스트 모드용)
// ============================================================================

static esp_err_t handle_tally_state_changed(const event_data_t* event)
{
    if (!event || !s_app.initialized) {
        return ESP_OK;
    }

    // 테스트 모드 실행 중인지 확인
    bool test_mode_running = tally_test_service_is_running();

    // 테스트 모드가 실행 중이 아니면 스킵 (스위처 Tally는 SwitcherService에서 처리)
    if (!test_mode_running) {
        return ESP_OK;
    }

    // 데이터 크기 확인
    if (event->data_size < sizeof(tally_event_data_t)) {
        return ESP_OK;
    }

    const tally_event_data_t* tally_event = (const tally_event_data_t*)event->data;
    if (!tally_event) {
        return ESP_OK;
    }

    // 채널 수 유효성 확인
    if (tally_event->channel_count == 0 || tally_event->channel_count > TALLY_MAX_CHANNELS) {
        return ESP_OK;
    }

    // packed_data_t 생성 (tally_data는 내부 배열이므로 복사 필요 없음)
    packed_data_t tally;
    tally.channel_count = tally_event->channel_count;
    tally.data_size = (tally_event->channel_count + 3) / 4;
    tally.data = (uint8_t*)tally_event->tally_data;

    char hex_str[16];
    packed_data_to_hex(&tally, hex_str, sizeof(hex_str));

    // 테스트 모드에서는 라이선스 확인 패스
    esp_err_t ret = lora_service_send_tally(&tally);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "LoRa TX (test mode): [F1][%d][%s] (%d channels, %d bytes)",
                 tally.channel_count, hex_str, tally.channel_count, tally.data_size);
    } else {
        T_LOGE(TAG, "LoRa TX failed: [%s] -> %s", hex_str, esp_err_to_name(ret));
    }

    return ESP_OK;
}

// ============================================================================
// 네트워크/스위처 연결 상태 변경 핸들러 (즉시 TxPage 갱신)
// ============================================================================

static esp_err_t handle_switcher_connected(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Switcher connected");
    return ESP_OK;
}

static esp_err_t handle_switcher_disconnected(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Switcher disconnected");
    return ESP_OK;
}

static esp_err_t handle_network_connected(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Network connected");

    // 디스플레이에 네트워크 상태 갱신
    network_status_t net_status = network_service_get_status();

    // WiFi STA 연결 시 디스플레이 갱신
    if (net_status.wifi_sta.connected) {
        tx_page_set_wifi_ip(net_status.wifi_sta.ip);
        tx_page_set_wifi_connected(true);
    }

    // Ethernet 연결 시 디스플레이 갱신
    if (net_status.ethernet.connected) {
        tx_page_set_eth_ip(net_status.ethernet.ip);
        tx_page_set_eth_connected(true);
    }

    return ESP_OK;
}

static esp_err_t handle_network_disconnected(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Network disconnected");

    // 디스플레이 상태는 DisplayManager에서 처리
    // 스위처 재연결은 EVT_NETWORK_STATUS_CHANGED 이벤트로 SwitcherService에서 처리

    return ESP_OK;
}

extern "C" {

bool prod_tx_app_init(const prod_tx_config_t* config)
{
    (void)config;

    if (s_app.initialized) {
        T_LOGW(TAG, "Already initialized");
        return true;
    }

    T_LOGI(TAG, "TX app init...");
    T_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);

    esp_err_t ret;

    // 네트워크 스택 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "Event loop creation failed: %s", esp_err_to_name(ret));
        return false;
    }

    // event_bus 초기화
    ret = event_bus_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "EventBus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 모든 이벤트 구독 (init 단계에서 일괄 구독)
    // 네트워크 이벤트
    event_bus_subscribe(EVT_NETWORK_DISCONNECTED, handle_network_disconnected);
    event_bus_subscribe(EVT_NETWORK_CONNECTED, handle_network_connected);
    // 스위처 연결 상태 이벤트
    event_bus_subscribe(EVT_SWITCHER_CONNECTED, handle_switcher_connected);
    event_bus_subscribe(EVT_SWITCHER_DISCONNECTED, handle_switcher_disconnected);
#ifdef DEVICE_MODE_TX
    // 버튼 이벤트
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);
#endif
    // 테스트 모드 이벤트
    event_bus_subscribe(EVT_TALLY_TEST_MODE_START, handle_test_mode_start);
    event_bus_subscribe(EVT_TALLY_TEST_MODE_STOP, handle_test_mode_stop);
    // Tally 상태 변경 이벤트 (테스트 모드 포함)
    event_bus_subscribe(EVT_TALLY_STATE_CHANGED, handle_tally_state_changed);
    // 배터리 엠티 체크 (1초마다 HardwareService에서 EVT_INFO_UPDATED 발행)
    event_bus_subscribe(EVT_INFO_UPDATED, [](const event_data_t* event) -> esp_err_t {
        (void)event;
        check_battery_empty();
        return ESP_OK;
    });
    T_LOGD(TAG, "Event subscription completed");

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

    // LicenseService 초기화
    ret = license_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "LicenseService init failed: %s", esp_err_to_name(ret));
        return false;
    }
    ret = license_service_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "LicenseService start failed: %s", esp_err_to_name(ret));
        return false;
    }
    T_LOGI(TAG, "LicenseService initialization completed");

    // device_limit 적용 (초과분 삭제)
    config_service_apply_device_limit();

    // 네트워크 설정 확인 (비어있으면 기본값 저장)
    config_all_t current_config;
    ret = config_service_load_all(&current_config);
    if (ret != ESP_OK || strlen(current_config.wifi_ap.ssid) == 0) {
        T_LOGI(TAG, "No network config, saving defaults");
        config_service_load_defaults(&current_config);
        config_service_save_all(&current_config);
    }

    // NetworkService 초기화 (이벤트 기반, EVT_CONFIG_DATA_CHANGED 대기)
    ret = network_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NetworkService init failed: %s", esp_err_to_name(ret));
        return false;
    }
    T_LOGI(TAG, "NetworkService initialized (event-based)");

    // SwitcherService 생성 (이벤트 기반 설정)
    s_app.service = switcher_service_create();
    if (!s_app.service) {
        T_LOGE(TAG, "SwitcherService creation failed");
        return false;
    }

    // 콜백 설정 (LoRa 송신은 Service 레이어에서 직접 처리)
    // switcher_service_set_tally_callback 불필요 - onSwitcherTallyChange에서 송신
    switcher_service_set_connection_callback(s_app.service, on_connection_change);
    switcher_service_set_switcher_change_callback(s_app.service, on_switcher_change);

    // 태스크 시작 (EVT_CONFIG_DATA_CHANGED에서 어댑터 자동 생성)
    if (!switcher_service_start(s_app.service)) {
        T_LOGE(TAG, "SwitcherService task start failed");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }
    T_LOGI(TAG, "SwitcherService task started (event-based config)");

    // LoRa 초기화 (NVS에 저장된 RF 설정 사용)
    config_device_t device_config;
    // 칩 감지 후 기본값 결정
    lora_chip_type_t chip = lora_driver_detect_chip();

    // BootPage에 칩 타입 전달 (주파수 표시용)
    boot_page_set_chip_type((uint8_t)chip);

    if (config_service_get_device(&device_config, chip) == ESP_OK) {
        T_LOGI(TAG, "RF config loaded: %.1f MHz, Sync 0x%02X, SF%d, CR%d, BW%.0f, TXP%ddBm",
                 device_config.rf.frequency, device_config.rf.sync_word,
                 device_config.rf.sf, device_config.rf.cr,
                 device_config.rf.bw, device_config.rf.tx_power);
    } else {
        T_LOGE(TAG, "RF config load failed");
        return false;
    }

    lora_service_config_t lora_config = {
        .frequency = device_config.rf.frequency,
        .spreading_factor = device_config.rf.sf,
        .coding_rate = device_config.rf.cr,
        .bandwidth = device_config.rf.bw,
        .tx_power = device_config.rf.tx_power,
        .sync_word = device_config.rf.sync_word
    };
    esp_err_t lora_ret = lora_service_init(&lora_config);
    if (lora_ret != ESP_OK) {
        T_LOGW(TAG, "LoRa init failed: %s", esp_err_to_name(lora_ret));
    } else {
        T_LOGI(TAG, "LoRa init complete (event-based config)");
    }

    // DisplayManager 초기화 (TxPage 자동 등록됨)
    if (!display_manager_init()) {
        T_LOGE(TAG, "DisplayManager init failed");
        return false;
    }

    // 버튼 서비스 초기화
    ret = button_service_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Button service init failed: %s", esp_err_to_name(ret));
    }

    // WebServer 초기화 (이벤트 구독)
    ret = web_server_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "WebServer init failed: %s", esp_err_to_name(ret));
    }

    // TallyTestService 초기화
    ret = tally_test_service_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "TallyTestService init failed: %s", esp_err_to_name(ret));
    }

    // DeviceManager 초기화 (이벤트 구독)
    ret = device_manager_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "DeviceManager init failed: %s", esp_err_to_name(ret));
    }

    s_app.initialized = true;
    T_LOGI(TAG, "TX app init complete");

    // 설정 로그 (Switcher는 이미 위에서 출력됨)
    T_LOGI(TAG, "  Frequency: %.1f MHz", lora_config.frequency);
    T_LOGI(TAG, "  SF: %d, CR: 4/%d, BW: %.0f kHz",
             lora_config.spreading_factor,
             lora_config.coding_rate,
             lora_config.bandwidth);

    return true;
}

void prod_tx_app_start(void)
{
    if (!s_app.initialized) {
        T_LOGE(TAG, "Not initialized");
        return;
    }

    if (s_app.running) {
        T_LOGW(TAG, "Already running");
        return;
    }

    // 모든 서비스는 init()에서 자동으로 start() 호출됨
    // 여기서는 추가 설정만 수행

    // LicenseService 재시작 (이벤트 재발행하여 device_manager에서 device_limit 초기화)
    license_service_start();
    T_LOGI(TAG, "LicenseService restarted (device_limit event published)");

    // DisplayManager BootPage로 전환 (이미 init에서 시작됨)
    display_manager_set_page(PAGE_BOOT);

    // 저장된 RF 설정 로드 및 이벤트 발행 (DisplayManager 구독 완료 후)
    config_all_t saved_config;
    esp_err_t ret = config_service_load_all(&saved_config);
    if (ret == ESP_OK) {
        // RF 설정 이벤트 발행 (DisplayManager용, 드라이버는 init에서 이미 설정됨)
        lora_rf_event_t rf_event = {
            .frequency = saved_config.device.rf.frequency,
            .sync_word = saved_config.device.rf.sync_word
        };
        event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
        T_LOGD(TAG, "RF config event published: %.1f MHz, Sync 0x%02X",
                 rf_event.frequency, rf_event.sync_word);
    } else {
        T_LOGW(TAG, "RF config load failed: %s", esp_err_to_name(ret));
    }

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

    // 배터리 정상이면 TX 페이지로 전환
    if (!battery_check_ok || battery_status.voltage >= 3.2f) {
        display_manager_boot_complete();
    }

    // WebServer 시작 (HTTP 서버)
    if (web_server_start() == ESP_OK) {
        T_LOGI(TAG, "WebServer started");
    } else {
        T_LOGW(TAG, "WebServer start failed");
    }

    s_app.running = true;
    T_LOGI(TAG, "TX app started");
}

void prod_tx_app_stop(void)
{
    if (!s_app.running) {
        return;
    }

    // WebServer 중지
    web_server_stop();

    // DeviceManager 중지
    device_manager_stop();

#ifdef DEVICE_MODE_TX
    // 버튼 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_unsubscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_unsubscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);
#endif

    // 스위처/네트워크 연결 상태 이벤트 구독 취소
    event_bus_unsubscribe(EVT_SWITCHER_CONNECTED, handle_switcher_connected);
    event_bus_unsubscribe(EVT_SWITCHER_DISCONNECTED, handle_switcher_disconnected);
    event_bus_unsubscribe(EVT_NETWORK_CONNECTED, handle_network_connected);
    event_bus_unsubscribe(EVT_NETWORK_DISCONNECTED, handle_network_disconnected);

    // 테스트 모드 이벤트 구독 취소
    event_bus_unsubscribe(EVT_TALLY_TEST_MODE_START, handle_test_mode_start);
    event_bus_unsubscribe(EVT_TALLY_TEST_MODE_STOP, handle_test_mode_stop);
    // Tally 상태 변경 이벤트 구독 취소
    event_bus_unsubscribe(EVT_TALLY_STATE_CHANGED, handle_tally_state_changed);

    button_service_stop();

    // LoRa 정지
    lora_service_stop();

    // DisplayManager 정지
    display_manager_stop();

    // NetworkService 정지
    network_service_stop();

    s_app.running = false;
    T_LOGI(TAG, "TX app stopped");
}

void prod_tx_app_deinit(void)
{
    prod_tx_app_stop();

    button_service_deinit();

    if (s_app.service) {
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
    }

    // LoRa 정리
    lora_service_deinit();

    // NetworkService 정리
    network_service_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "TX app deinit complete");
}

void prod_tx_app_loop(void)
{
    // 각 서비스가 자체 태스크에서 실행되므로 루프에서 처리할 내용 없음
    // - DisplayManager: 내부 태스크에서 주기적 갱신
    // - NetworkService: 내부 태스크에서 상태 발행
    // - SwitcherService: 내부 태스크에서 루프 처리
    // - HardwareService: 내부 태스크에서 모니터링
    // - 배터리 엠티 체크: EVT_INFO_UPDATED 이벤트로 처리
    (void)s_app.running;
}

void prod_tx_app_print_status(void)
{
    T_LOGI(TAG, "===== TX App Status =====");
    T_LOGI(TAG, "Running: %s", s_app.running ? "Yes" : "No");

    if (s_app.service) {
        // Primary 상태
        switcher_status_t primary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_PRIMARY);
        T_LOGI(TAG, "  Primary: %s, cameras=%d",
                 connection_state_to_string(primary_status.state),
                 primary_status.camera_count);

        // Secondary 상태
        if (switcher_service_is_dual_mode_enabled(s_app.service)) {
            switcher_status_t secondary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_SECONDARY);
            T_LOGI(TAG, "  Secondary: %s, cameras=%d",
                     connection_state_to_string(secondary_status.state),
                     secondary_status.camera_count);
        }

        T_LOGI(TAG, "Dual mode: %s",
                 switcher_service_is_dual_mode_enabled(s_app.service) ? "enabled" : "disabled");
    }

    T_LOGI(TAG, "=========================");
}

bool prod_tx_app_is_running(void)
{
    return s_app.running;
}

bool prod_tx_app_is_connected(void)
{
    if (!s_app.initialized || !s_app.service) {
        return false;
    }

    // Primary 연결 확인
    switcher_status_t primary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_PRIMARY);
    if (primary_status.state != CONNECTION_STATE_READY &&
        primary_status.state != CONNECTION_STATE_CONNECTED) {
        return false;
    }

    // Secondary 연결 확인 (듀얼모드인 경우)
    if (switcher_service_is_dual_mode_enabled(s_app.service)) {
        switcher_status_t secondary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_SECONDARY);
        if (secondary_status.state != CONNECTION_STATE_READY &&
            secondary_status.state != CONNECTION_STATE_CONNECTED) {
            return false;
        }
    }

    return true;
}

} // extern "C"
