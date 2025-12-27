/**
 * @file prod_rx_app.cpp
 * @brief 프로덕션 Tally 수신 앱 구현
 */

#include "prod_rx_app.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "event_bus.h"
#include "ConfigService.h"
#include "DisplayManager.h"
#include "ButtonService.h"
#include "LoRaService.h"
#include "LedService.h"
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
// LoRa 패킷 수신 처리 (RSSI/SNR 포함)
// ============================================================================

// 정적 버퍼 (이벤트 발행 후에도 데이터 유지)
static uint8_t s_tally_buffer[8];  // 최대 20채널 = 5바이트
static tally_event_data_t s_tally_event;  // 정적 이벤트 데이터

/**
 * @brief LoRa 패킷 이벤트 핸들러 (RSSI/SNR 포함)
 */
static esp_err_t on_lora_packet_event(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_OK;
    }

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;
    const uint8_t* data = packet->data;
    size_t len = packet->length;
    int16_t rssi = packet->rssi;
    float snr = packet->snr;

    if (!data || len == 0) {
        return ESP_OK;
    }

    // 패킷 구조: [F1][ChannelCount][Data...]
    // - F1: 고정 헤더
    // - ChannelCount: 실제 채널 수 (1-20)
    // - Data: packed tally 데이터

    // 헤더 검증
    if (data[0] != 0xF1) {
        T_LOGW(TAG, "알 수 없는 헤더: 0x%02X", data[0]);
        return ESP_OK;
    }

    // 길이 체크 (최소 2바이트: F1 + ChannelCount)
    if (len < 2) {
        T_LOGW(TAG, "패킷 길이 부족: %d", (int)len);
        return ESP_OK;
    }

    uint8_t ch_count = data[1];
    if (ch_count < 1 || ch_count > 20) {
        T_LOGW(TAG, "잘못된 채널 수: %d", ch_count);
        return ESP_OK;
    }

    // 데이터 길이 계산
    uint8_t expected_data_len = (ch_count + 3) / 4;
    size_t payload_len = len - 2;  // 헤더(2) 제외

    if (payload_len != expected_data_len || payload_len > sizeof(s_tally_buffer)) {
        T_LOGW(TAG, "데이터 길이 불일치: 예상 %d, 수신 %d", expected_data_len, (int)payload_len);
        return ESP_OK;
    }

    const uint8_t* payload = &data[2];

    // 정적 버퍼에 데이터 복사 (이벤트 발행 후에도 유효)
    memcpy(s_tally_buffer, payload, payload_len);

    // packed_data_t로 변환 (정적 버퍼 사용)
    packed_data_t tally = {
        .data = s_tally_buffer,
        .data_size = static_cast<uint8_t>(payload_len),
        .channel_count = ch_count  // 실제 채널 수 사용
    };

    if (!packed_data_is_valid(&tally)) {
        T_LOGW(TAG, "잘못된 Tally 데이터");
        return ESP_OK;
    }

    // 헥스 문자열 변환 (데이터만)
    char hex_str[16];
    packed_data_to_hex(&tally, hex_str, sizeof(hex_str));

    // Tally 상태 문자열 변환
    char tally_str[64];
    packed_data_format_tally(&tally, tally_str, sizeof(tally_str));

    T_LOGI(TAG, "LoRa 수신: [F1][%d][%s] (%d채널, %d바이트) → %s RSSI:%d SNR:%.1f",
             ch_count, hex_str, ch_count, (int)payload_len, tally_str, rssi, snr);

    // RSSI/SNR은 주기적 업데이트(1초)만 사용 (패킷 수신 시 즉시 업데이트 제거)
    // display_manager_update_rssi(rssi, snr);

    // RxPage용 PGM/PVW 채널 추출
    uint8_t pgm_channels[20];
    uint8_t pvw_channels[20];
    uint8_t pgm_count = 0;
    uint8_t pvw_count = 0;

    for (uint8_t i = 0; i < ch_count && i < 20; i++) {
        uint8_t status = packed_data_get_channel(&tally, i + 1);
        if (status == TALLY_STATUS_PROGRAM || status == TALLY_STATUS_BOTH) {
            pgm_channels[pgm_count++] = i + 1;
        }
        if (status == TALLY_STATUS_PREVIEW || status == TALLY_STATUS_BOTH) {
            pvw_channels[pvw_count++] = i + 1;
        }
    }

    // Tally 상태 변경 이벤트 발행 (WS2812 먼저 반응)
    // 정적 변수 사용 (이벤트 버스가 포인터만 복사하므로)
    s_tally_event.source = SWITCHER_ROLE_PRIMARY;
    s_tally_event.channel_count = ch_count;
    s_tally_event.tally_value = packed_data_to_uint64(&tally);
    memcpy(s_tally_event.tally_data, s_tally_buffer, payload_len);

    event_bus_publish(EVT_TALLY_STATE_CHANGED, &s_tally_event, sizeof(s_tally_event));

    // DisplayManager에 Tally 데이터 업데이트 (즉시 갱신됨)
    display_manager_update_tally(pgm_channels, pgm_count, pvw_channels, pvw_count);

    return ESP_OK;
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
// 버튼 이벤트 핸들러
// ============================================================================

static esp_err_t handle_button_single_click(const event_data_t* event)
{
    (void)event;

#ifdef DEVICE_MODE_RX
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
#else
    // TxPage: 1 -> 2 -> 3 -> 4 -> 5 -> 1 순환
    uint8_t current = display_manager_get_page_index();
    uint8_t next = (current == 5) ? 1 : (current + 1);
    display_manager_switch_page(next);
    display_manager_force_refresh();
    T_LOGI(TAG, "TxPage: %d -> %d", current, next);
#endif

    return ESP_OK;
}

static esp_err_t handle_button_long_press(const event_data_t* event)
{
    (void)event;

#ifdef DEVICE_MODE_RX
    // 롱 프레스: RX 모드에서 카메라 ID 변경 팝업 표시
    if (display_manager_get_state() == 0) {
        uint8_t max_camera = config_service_get_max_camera_num();
        display_manager_show_camera_id_popup(max_camera);
        display_manager_set_camera_id_changing(true);
        start_camera_id_timer();
        display_manager_force_refresh();
        T_LOGI(TAG, "Camera ID 팝업 표시 (롱프레스, max: %d)", max_camera);
    }
#else
    T_LOGI(TAG, "Long press (TX mode: no action)");
#endif

    return ESP_OK;
}

static esp_err_t handle_button_long_release(const event_data_t* event)
{
    (void)event;

#ifdef DEVICE_MODE_RX
    // 롱 프레스 해제: RX 모드에서 카메라 ID 저장
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
#else
    T_LOGD(TAG, "Long press release");
#endif

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
        T_LOGE(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
        return false;
    }

    // WS2812 LED 초기화
    uint8_t camera_id = config_service_get_camera_id();
    esp_err_t led_ret = led_service_init(-1, 0, camera_id);
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

    // LoRa 시작
    lora_service_start();
    T_LOGI(TAG, "LoRa 시작");

    // LoRa 패킷 수신 이벤트 구독 (RSSI/SNR 포함)
    event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_event);

    // DisplayManager 시작, BootPage로 전환
    display_manager_start();
    display_manager_set_page(PAGE_BOOT);

    // 버튼 이벤트 구독
    event_bus_subscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_subscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

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

    // 버튼 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BUTTON_SINGLE_CLICK, handle_button_single_click);
    event_bus_unsubscribe(EVT_BUTTON_LONG_PRESS, handle_button_long_press);
    event_bus_unsubscribe(EVT_BUTTON_LONG_RELEASE, handle_button_long_release);

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

        // RSSI/SNR은 LoRaService 이벤트로 업데이트됨
        display_manager_update_rssi(config_service_get_rssi(),
                                   config_service_get_snr());
    }
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
