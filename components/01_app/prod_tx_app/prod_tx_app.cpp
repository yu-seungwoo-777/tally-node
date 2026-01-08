/**
 * @file prod_tx_app.cpp
 * @brief 프로덕션 Tally 송신 앱 구현
 */

#include "prod_tx_app.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "event_bus.h"
#include "config_service.h"
#include "hardware_service.h"
#include "license_service.h"
#include "DisplayManager.h"
#include "TxPage.h"
#include "button_service.h"
#include "switcher_service.h"
#include "network_service.h"
#include "lora_service.h"
#include "device_manager.h"
#include "web_server.h"
#include "TallyTypes.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

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
// LoRa 송신 헬퍼
// ============================================================================

/**
 * @brief LoRa로 Tally 데이터 송신
 * 패킷 구조: [F1][ChannelCount][Data...]
 */
static void send_tally_via_lora(const packed_data_t* tally)
{
    if (!packed_data_is_valid(tally)) {
        T_LOGW(TAG, "LoRa 송신 스킵: 잘못된 Tally 데이터");
        return;
    }

    // 라이센스 확인
    if (!license_service_can_send_tally()) {
        T_LOGW(TAG, "LoRa 송신 스킵: 라이센스 미인증 상태");
        return;
    }

    char hex_str[16];
    packed_data_to_hex(tally, hex_str, sizeof(hex_str));

    esp_err_t ret = lora_service_send_tally(tally);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "LoRa 송신: [F1][%d][%s] (%d채널, %d바이트)",
                 tally->channel_count, hex_str, tally->channel_count, tally->data_size);
    } else {
        T_LOGE(TAG, "LoRa 송신 실패: [%s] -> %s", hex_str, esp_err_to_name(ret));
    }
}

// ============================================================================
// 버튼 이벤트 핸들러 (TX 전용)
// ============================================================================

#ifdef DEVICE_MODE_TX
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
    T_LOGI(TAG, "Long press -> 라이센스 검증 시도");

    // 저장된 라이센스 키로 검증
    char license_key[17] = {0};
    if (license_service_get_key(license_key) == ESP_OK && strlen(license_key) == 16) {
        T_LOGI(TAG, "저장된 라이센스 키로 검증: %.16s", license_key);
        license_service_validate(license_key);
    } else {
        T_LOGW(TAG, "라이센스 키 없음, 검증 스킵");
    }
    return ESP_OK;
}

static esp_err_t handle_button_long_release(const event_data_t* event)
{
    (void)event;
    T_LOGD(TAG, "Long press release");
    return ESP_OK;
}

/**
 * @brief 디바이스 밝기 설정 요청 핸들러
 * 이벤트 데이터: uint8_t[3] = {device_id[0], device_id[1], brightness}
 */
static esp_err_t handle_device_brightness_request(const event_data_t* event)
{
    if (!event || event->data_size < 3) {
        T_LOGW(TAG, "밝기 요청 이벤트 데이터 부족");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = event->data;
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t brightness = data[2];

    // 라이센스 확인
    if (!license_service_can_send_tally()) {
        T_LOGW(TAG, "밝기 명령 송신 스킵: 라이센스 미인증 상태");
        return ESP_ERR_INVALID_STATE;
    }

    // LoRa 밝기 설정 명령 패킷 구성
    // [0xE1][device_id[0]][device_id[1]][brightness]
    uint8_t pkt[4];
    pkt[0] = 0xE1;  // LORA_HDR_SET_BRIGHTNESS
    pkt[1] = device_id[0];
    pkt[2] = device_id[1];
    pkt[3] = brightness;

    esp_err_t ret = lora_service_send(pkt, sizeof(pkt));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "밝기 명령 LoRa 송신: ID[%02X%02X], 밝기=%d",
                 device_id[0], device_id[1], brightness);
    } else {
        T_LOGE(TAG, "밝기 명령 LoRa 송신 실패: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 디바이스 카메라 ID 설정 요청 핸들러
 * 이벤트 데이터: uint8_t[3] = {device_id[0], device_id[1], camera_id}
 */
static esp_err_t handle_device_camera_id_request(const event_data_t* event)
{
    if (!event || event->data_size < 3) {
        T_LOGW(TAG, "카메라 ID 요청 이벤트 데이터 부족");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = event->data;
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t camera_id = data[2];

    // 라이센스 확인
    if (!license_service_can_send_tally()) {
        T_LOGW(TAG, "카메라 ID 명령 송신 스킵: 라이센스 미인증 상태");
        return ESP_ERR_INVALID_STATE;
    }

    // LoRa 카메라 ID 설정 명령 패킷 구성
    // [0xE2][device_id[0]][device_id[1]][camera_id]
    uint8_t pkt[4];
    pkt[0] = 0xE2;  // LORA_HDR_SET_CAMERA_ID
    pkt[1] = device_id[0];
    pkt[2] = device_id[1];
    pkt[3] = camera_id;

    esp_err_t ret = lora_service_send(pkt, sizeof(pkt));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "카메라 ID 명령 LoRa 송신: ID[%02X%02X], CameraID=%d",
                 device_id[0], device_id[1], camera_id);
    } else {
        T_LOGE(TAG, "카메라 ID 명령 LoRa 송신 실패: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 디바이스 PING 요청 핸들러
 * 이벤트 데이터: uint8_t[2] = {device_id[0], device_id[1]}
 */
static esp_err_t handle_device_ping_request(const event_data_t* event)
{
    if (!event || event->data_size < 2) {
        T_LOGW(TAG, "PING 요청 이벤트 데이터 부족");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* device_id = event->data;

    // PING 명령 패킷 구성 (send_ping_command 호출 또는 직접 전송)
    // [0xE6][device_id[0]][device_id[1]][timestamp_low]
    uint8_t pkt[4];
    pkt[0] = 0xE6;  // LORA_HDR_PING
    pkt[1] = device_id[0];
    pkt[2] = device_id[1];
    pkt[3] = (uint8_t)(xTaskGetTickCount() * portTICK_PERIOD_MS & 0xFF);  // timestamp 하위 바이트만 사용

    esp_err_t ret = lora_service_send(pkt, sizeof(pkt));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "PING 명령 LoRa 송신: ID[%02X%02X]",
                 device_id[0], device_id[1]);
    } else {
        T_LOGE(TAG, "PING 명령 LoRa 송신 실패: %s", esp_err_to_name(ret));
    }

    return ret;
}
#endif // DEVICE_MODE_TX

// ============================================================================
// 앱 상태
// ============================================================================

static struct {
    switcher_service_handle_t service;
    bool running;
    bool initialized;
    packed_data_t last_tally;
} s_app = {
    .service = nullptr,
    .running = false,
    .initialized = false,
    .last_tally = {nullptr, 0, 0}
};

// ============================================================================
// SwitcherService 콜백 핸들러
// ============================================================================

static void on_tally_change(void)
{
    // Tally 데이터 변경 시 즉시 LoRa 송신
    if (!s_app.initialized || !s_app.service) {
        return;
    }

    packed_data_t tally = switcher_service_get_combined_tally(s_app.service);
    send_tally_via_lora(&tally);

    // 마지막 Tally 저장
    if (s_app.last_tally.data) {
        packed_data_cleanup(&s_app.last_tally);
    }
    packed_data_copy(&s_app.last_tally, &tally);
}

static void on_connection_change(connection_state_t state)
{
    T_LOGI(TAG, "연결 상태 변경: %s", connection_state_to_string(state));
}

static void on_switcher_change(switcher_role_t role)
{
    T_LOGI(TAG, "%s 스위처 변경 감지", switcher_role_to_string(role));
}

// ============================================================================
// 네트워크/스위처 연결 상태 변경 핸들러 (즉시 TxPage 갱신)
// ============================================================================

static esp_err_t handle_switcher_connected(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "스위처 연결됨 -> TxPage 갱신");
    // 주기적 갱신에 의해 처리되므로 즉시 갱신 불필요 (1초 내 반영)
    return ESP_OK;
}

static esp_err_t handle_switcher_disconnected(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "스위처 연결 해제 -> TxPage 갱신");
    return ESP_OK;
}

static esp_err_t handle_network_connected(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "네트워크 연결됨 -> 스위처 재연결, 디스플레이 갱신");
    if (s_app.service) {
        switcher_service_reconnect_all(s_app.service);
    }

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
    T_LOGI(TAG, "네트워크 연결 해제 -> 디스플레이 갱신");

    // 디스플레이에 네트워크 상태 갱신
    tx_page_set_wifi_connected(false);
    tx_page_set_eth_connected(false);

    // 스위처 재연결 시도
    if (s_app.service) {
        switcher_service_reconnect_all(s_app.service);
    }

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

    // NVS 초기화
    esp_err_t ret = init_nvs();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 네트워크 스택 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "esp_netif_init 실패: %s", esp_err_to_name(ret));
        return false;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "이벤트 루프 생성 실패: %s", esp_err_to_name(ret));
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
    // 디바이스 제어 이벤트
    event_bus_subscribe(EVT_DEVICE_BRIGHTNESS_REQUEST, handle_device_brightness_request);
    event_bus_subscribe(EVT_DEVICE_CAMERA_ID_REQUEST, handle_device_camera_id_request);
    event_bus_subscribe(EVT_DEVICE_PING_REQUEST, handle_device_ping_request);
#endif
    T_LOGI(TAG, "이벤트 구독 완료");

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
    T_LOGI(TAG, "LicenseService 초기화 완료");

    // 네트워크 설정 확인 (비어있으면 기본값 저장)
    config_all_t current_config;
    ret = config_service_load_all(&current_config);
    if (ret != ESP_OK || strlen(current_config.wifi_ap.ssid) == 0) {
        T_LOGI(TAG, "네트워크 설정 없음, 기본값 저장");
        config_service_load_defaults(&current_config);
        config_service_save_all(&current_config);
    }

    // NetworkService 초기화 (이벤트 기반, EVT_CONFIG_DATA_CHANGED 대기)
    ret = network_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "NetworkService init failed: %s", esp_err_to_name(ret));
        return false;
    }
    T_LOGI(TAG, "NetworkService 초기화 완료 (이벤트 기반)");

    // SwitcherService 생성 (이벤트 기반 설정)
    s_app.service = switcher_service_create();
    if (!s_app.service) {
        T_LOGE(TAG, "SwitcherService 생성 실패");
        return false;
    }

    // 콜백 설정
    switcher_service_set_tally_callback(s_app.service, on_tally_change);
    switcher_service_set_connection_callback(s_app.service, on_connection_change);
    switcher_service_set_switcher_change_callback(s_app.service, on_switcher_change);

    // 태스크 시작 (EVT_CONFIG_DATA_CHANGED에서 어댑터 자동 생성)
    if (!switcher_service_start(s_app.service)) {
        T_LOGE(TAG, "SwitcherService 태스크 시작 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }
    T_LOGI(TAG, "SwitcherService 태스크 시작 (이벤트 기반 설정)");

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
        T_LOGW(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
    } else {
        T_LOGI(TAG, "LoRa 초기화 완료 (이벤트 기반 설정)");
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

    // DeviceManager 초기화 (이벤트 구독)
    ret = device_manager_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "DeviceManager init failed: %s", esp_err_to_name(ret));
    }

    s_app.initialized = true;
    T_LOGI(TAG, "TX app init complete");

    // 설정 로그 (Switcher는 이미 위에서 출력됨)
    T_LOGI(TAG, "  주파수: %.1f MHz", lora_config.frequency);
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

    // HardwareService 시작 (모니터링 태스크)
    hardware_service_start();
    T_LOGI(TAG, "HardwareService 시작");

    // LoRa 시작
    lora_service_start();
    T_LOGI(TAG, "LoRa 시작");

    // DeviceManager 시작 (상태 요청 태스크)
    device_manager_start();
    T_LOGI(TAG, "DeviceManager 시작");

    // DisplayManager 시작, BootPage로 전환
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 저장된 RF 설정 로드 및 이벤트 발행 (lora_service, DisplayManager 구독 완료 후)
    config_all_t saved_config;
    esp_err_t ret = config_service_load_all(&saved_config);
    if (ret == ESP_OK) {
        // RF 설정 이벤트 발행 (frequency, sync_word)
        lora_rf_event_t rf_event = {
            .frequency = saved_config.device.rf.frequency,
            .sync_word = saved_config.device.rf.sync_word
        };
        event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
        T_LOGI(TAG, "RF 설정 이벤트 발행: %.1f MHz, Sync 0x%02X",
                 rf_event.frequency, rf_event.sync_word);
    } else {
        T_LOGW(TAG, "RF 설정 로드 실패: %s", esp_err_to_name(ret));
    }

    // 버튼 서비스 시작
    button_service_start();

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

    // WebServer 시작 (HTTP 서버)
    if (web_server_start() == ESP_OK) {
        T_LOGI(TAG, "WebServer 시작");
    } else {
        T_LOGW(TAG, "WebServer 시작 실패");
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
    // 디바이스 제어 이벤트 구독 취소
    event_bus_unsubscribe(EVT_DEVICE_BRIGHTNESS_REQUEST, handle_device_brightness_request);
    event_bus_unsubscribe(EVT_DEVICE_CAMERA_ID_REQUEST, handle_device_camera_id_request);
    event_bus_unsubscribe(EVT_DEVICE_PING_REQUEST, handle_device_ping_request);
#endif

    // 스위처/네트워크 연결 상태 이벤트 구독 취소
    event_bus_unsubscribe(EVT_SWITCHER_CONNECTED, handle_switcher_connected);
    event_bus_unsubscribe(EVT_SWITCHER_DISCONNECTED, handle_switcher_disconnected);
    event_bus_unsubscribe(EVT_NETWORK_CONNECTED, handle_network_connected);
    event_bus_unsubscribe(EVT_NETWORK_DISCONNECTED, handle_network_disconnected);

    button_service_stop();

    // LoRa 정지
    lora_service_stop();

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

    if (s_app.last_tally.data) {
        packed_data_cleanup(&s_app.last_tally);
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
    if (!s_app.running) {
        return;
    }

    // SwitcherService 루프 처리는 내부 태스크에서 자동 실행됨
    // Tally 변경 시 on_tally_change() 콜백을 통해 즉시 LoRa 송신

    // 디스플레이 갱신 (500ms 주기, DisplayManager 내부에서 체크)
    display_manager_update();

    // System 데이터는 HardwareService가 EVT_INFO_UPDATED로 발행 (1초마다)
    // Switcher/Network 데이터는 상태 변경 시 이벤트 발행
    // DisplayManager가 이벤트를 구독하여 자동 갱신됨

    // 네트워크 상태 이벤트 발행 (주기적 확인)
    static uint32_t last_network_check = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_network_check >= 1000) {
        last_network_check = now;
        network_service_publish_status();
    }
}

void prod_tx_app_print_status(void)
{
    T_LOGI(TAG, "===== TX App Status =====");
    T_LOGI(TAG, "Running: %s", s_app.running ? "Yes" : "No");

    if (s_app.service) {
        // Primary 상태
        switcher_status_t primary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_PRIMARY);
        T_LOGI(TAG, "  Primary: %s, 카메라=%d",
                 connection_state_to_string(primary_status.state),
                 primary_status.camera_count);

        // Secondary 상태
        if (switcher_service_is_dual_mode_enabled(s_app.service)) {
            switcher_status_t secondary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_SECONDARY);
            T_LOGI(TAG, "  Secondary: %s, 카메라=%d",
                     connection_state_to_string(secondary_status.state),
                     secondary_status.camera_count);
        }

        T_LOGI(TAG, "듀얼모드: %s",
                 switcher_service_is_dual_mode_enabled(s_app.service) ? "활성화" : "비활성화");
    }

    if (s_app.last_tally.data && s_app.last_tally.data_size > 0) {
        char hex_str[16];
        packed_data_to_hex(&s_app.last_tally, hex_str, sizeof(hex_str));
        T_LOGI(TAG, "마지막 Tally: [%s] (%d채널)",
                 hex_str, s_app.last_tally.channel_count);
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
