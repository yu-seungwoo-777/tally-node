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
#include "DisplayManager.h"
#include "TxPage.h"
#include "button_service.h"
#include "switcher_service.h"
#include "network_service.h"
#include "lora_service.h"
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
    switcher_service_handle_t service;
    bool running;
    bool initialized;
    packed_data_t last_tally;
    // Switcher 설정 (ConfigService에서 로드)
    config_switcher_t primary;
    config_switcher_t secondary;
    bool dual_mode;
    uint8_t secondary_offset;
} s_app = {
    .service = nullptr,
    .running = false,
    .initialized = false,
    .last_tally = {nullptr, 0, 0},
    .primary = {},     // zero-initialized (C++ style)
    .secondary = {},   // zero-initialized (C++ style)
    .dual_mode = false,
    .secondary_offset = 4
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
    T_LOGI(TAG, "네트워크 연결됨 -> 스위처 재연결");
    if (s_app.service) {
        switcher_service_reconnect_all(s_app.service);
    }
    return ESP_OK;
}

static esp_err_t handle_network_disconnected(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "네트워크 연결 해제 -> 스위처 재연결");
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

    // 네트워크 이벤트 구독 (재연결 트리거)
    event_bus_subscribe(EVT_NETWORK_DISCONNECTED, handle_network_disconnected);
    event_bus_subscribe(EVT_NETWORK_CONNECTED, handle_network_connected);

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

    // Switcher 설정 로드
    config_service_get_primary(&s_app.primary);
    config_service_get_secondary(&s_app.secondary);
    s_app.dual_mode = config_service_get_dual_enabled();
    s_app.secondary_offset = config_service_get_secondary_offset();

    // Switcher 설정 로그
    T_LOGI(TAG, "Switcher 설정 로드:");
    T_LOGI(TAG, "  Primary: %s (type=%d, if=%d, port=%d, limit=%d)",
             s_app.primary.ip, s_app.primary.type, s_app.primary.interface,
             s_app.primary.port, s_app.primary.camera_limit);
    T_LOGI(TAG, "  Secondary: %s (type=%d, if=%d, port=%d, limit=%d)",
             s_app.secondary.ip, s_app.secondary.type, s_app.secondary.interface,
             s_app.secondary.port, s_app.secondary.camera_limit);
    T_LOGI(TAG, "  Dual Mode: %s, Offset: %d",
             s_app.dual_mode ? "Enabled" : "Disabled", s_app.secondary_offset);

    // 네트워크 설정 확인 (비어있으면 기본값 저장)
    config_all_t current_config;
    ret = config_service_load_all(&current_config);
    if (ret != ESP_OK || strlen(current_config.wifi_ap.ssid) == 0) {
        T_LOGI(TAG, "네트워크 설정 없음, 기본값 저장");
        config_service_load_defaults(&current_config);
        config_service_save_all(&current_config);
    }

    // NetworkService 초기화 (스위처 통신을 위한 네트워크)
    esp_err_t net_ret = network_service_init();
    if (net_ret != ESP_OK) {
        T_LOGE(TAG, "NetworkService 초기화 실패: %s", esp_err_to_name(net_ret));
        return false;
    }
    T_LOGI(TAG, "NetworkService 초기화 완료");

    // SwitcherService 생성
    s_app.service = switcher_service_create();
    if (!s_app.service) {
        T_LOGE(TAG, "SwitcherService 생성 실패");
        return false;
    }

    // 콜백 설정
    switcher_service_set_tally_callback(s_app.service, on_tally_change);
    switcher_service_set_connection_callback(s_app.service, on_connection_change);
    switcher_service_set_switcher_change_callback(s_app.service, on_switcher_change);

    // Primary 스위처 설정 (ConfigService에서 로드한 값 사용)
    if (!switcher_service_set_atem(s_app.service,
                                     SWITCHER_ROLE_PRIMARY,
                                     "Primary",
                                     s_app.primary.ip,
                                     s_app.primary.port,
                                     s_app.primary.camera_limit,
                                     (tally_network_if_t)s_app.primary.interface)) {
        T_LOGE(TAG, "Primary 스위처 설정 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }
    T_LOGI(TAG, "Primary 스위처 설정 완료: %s:%d (if=%d, limit=%d)",
             s_app.primary.ip, s_app.primary.port, s_app.primary.interface, s_app.primary.camera_limit);

    // Secondary 스위처 설정 (듀얼모드인 경우)
    if (s_app.dual_mode && s_app.secondary.ip[0] != '\0') {
        if (!switcher_service_set_atem(s_app.service,
                                        SWITCHER_ROLE_SECONDARY,
                                        "Secondary",
                                        s_app.secondary.ip,
                                        s_app.secondary.port,
                                        s_app.secondary.camera_limit,
                                        (tally_network_if_t)s_app.secondary.interface)) {
            T_LOGW(TAG, "Secondary 스위처 설정 실패 (싱글모드로 동작)");
        } else {
            // 듀얼모드 설정
            switcher_service_set_dual_mode(s_app.service, true);
            switcher_service_set_secondary_offset(s_app.service, s_app.secondary_offset);
            T_LOGI(TAG, "듀얼모드 활성화 (offset: %d)", s_app.secondary_offset);
        }
    }

    // 서비스 초기화
    if (!switcher_service_initialize(s_app.service)) {
        T_LOGE(TAG, "SwitcherService 초기화 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }

    // 태스크 시작
    if (!switcher_service_start(s_app.service)) {
        T_LOGE(TAG, "SwitcherService 태스크 시작 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }
    T_LOGI(TAG, "SwitcherService 태스크 시작 (10ms 주기)");

    // LoRa 초기화 (ConfigService에서 RF 설정 가져오기)
    config_device_t device_config;
    config_service_get_device(&device_config);

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
        T_LOGW(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
    } else {
        T_LOGI(TAG, "LoRa 초기화 완료");
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

    // LoRa 시작
    lora_service_start();
    T_LOGI(TAG, "LoRa 시작");

    // DisplayManager 시작, BootPage로 전환
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

    // 스위처/네트워크 연결 상태 이벤트 구독
    event_bus_subscribe(EVT_SWITCHER_CONNECTED, handle_switcher_connected);
    event_bus_subscribe(EVT_SWITCHER_DISCONNECTED, handle_switcher_disconnected);
    event_bus_subscribe(EVT_NETWORK_CONNECTED, handle_network_connected);
    event_bus_subscribe(EVT_NETWORK_DISCONNECTED, handle_network_disconnected);

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

    s_app.running = true;
    T_LOGI(TAG, "TX app started");
}

void prod_tx_app_stop(void)
{
    if (!s_app.running) {
        return;
    }

    // 버튼 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_unsubscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_unsubscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

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

    // 디스플레이 갱신 (200ms 주기, DisplayManager 내부에서 체크)
    display_manager_update();

    // System 데이터 1000ms마다 갱신
    static uint32_t last_sys_update = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_sys_update >= 1000) {
        last_sys_update = now;

        // ========== HardwareService: System 데이터 ==========
        hardware_system_t sys;
        hardware_service_get_system(&sys);
        hardware_service_update_battery();  // ADC 읽기 (내부에서 battery 갱신)
        hardware_service_get_system(&sys);  // 갱신된 battery 다시 읽기

        // DisplayManager에 개별 파라미터 전달
        display_manager_update_system(sys.device_id, sys.battery,
                                      hardware_service_get_voltage(),
                                      hardware_service_get_temperature());

        // ========== ConfigService: RF 설정 ==========
        config_device_t device;
        config_service_get_device(&device);
        tx_page_set_frequency(device.rf.frequency);
        tx_page_set_sync_word(device.rf.sync_word);

        // ========== SwitcherService: S1/S2 상태 ==========
        if (s_app.service) {
            // 듀얼모드 설정
            tx_page_set_dual_mode(switcher_service_is_dual_mode_enabled(s_app.service));

            // S1 (Primary) 상태
            switcher_status_t s1_status = switcher_service_get_switcher_status(
                s_app.service, SWITCHER_ROLE_PRIMARY);
            bool s1_connected = (s1_status.state == CONNECTION_STATE_READY ||
                                 s1_status.state == CONNECTION_STATE_CONNECTED);
            tx_page_set_s1("ATEM", s_app.primary.ip, 9910, s1_connected);

            // S2 (Secondary) 상태 (듀얼모드인 경우)
            if (switcher_service_is_dual_mode_enabled(s_app.service)) {
                switcher_status_t s2_status = switcher_service_get_switcher_status(
                    s_app.service, SWITCHER_ROLE_SECONDARY);
                bool s2_connected = (s2_status.state == CONNECTION_STATE_READY ||
                                     s2_status.state == CONNECTION_STATE_CONNECTED);
                const char* s2_ip = (s_app.secondary.ip[0] != '\0')
                                    ? s_app.secondary.ip : "0.0.0.0";
                tx_page_set_s2("ATEM", s2_ip, 9910, s2_connected);
            }
        }

        // ========== NetworkService: AP/WiFi/Ethernet 상태 ==========
        network_status_t net_status = network_service_get_status();

        // AP 정보 (Page 2)
        config_wifi_ap_t ap_config;
        config_service_get_wifi_ap(&ap_config);
        tx_page_set_ap_name(ap_config.ssid);
        tx_page_set_ap_ip(net_status.wifi_ap.ip);

        // WiFi 정보 (Page 3)
        config_wifi_sta_t wifi_config;
        config_service_get_wifi_sta(&wifi_config);
        tx_page_set_wifi_ssid(wifi_config.ssid);
        tx_page_set_wifi_ip(net_status.wifi_sta.ip);
        tx_page_set_wifi_connected(net_status.wifi_sta.connected);

        // Ethernet 정보 (Page 4)
        config_ethernet_t eth_config;
        config_service_get_ethernet(&eth_config);
        tx_page_set_eth_ip(net_status.ethernet.ip);
        display_manager_update_ethernet_dhcp_mode(eth_config.dhcp_enabled);
        tx_page_set_eth_connected(net_status.ethernet.connected);
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
