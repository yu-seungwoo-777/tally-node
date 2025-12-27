/**
 * @file tally_rx_app.cpp
 * @brief Tally 수신 앱 구현
 */

#include "tally_rx_app.h"
#include "t_log.h"
#include "LoRaService.h"
#include "LedService.h"
#include "ConfigService.h"
#include "ButtonService.h"
#include "TallyTypes.h"
#include "event_bus.h"
#include <cstring>

// ============================================================================
// LoRa 수신 콜백
// ============================================================================

/**
 * @brief LoRa 수신 콜백
 * @param data 수신 데이터
 * @param len 데이터 길이
 */
static void on_lora_receive(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return;
    }

    // 패킷 구조: [F1][ChannelCount][Data...]
    // - F1: 고정 헤더
    // - ChannelCount: 실제 채널 수 (1-20)
    // - Data: packed tally 데이터

    // 헤더 검증
    if (data[0] != 0xF1) {
        T_LOGW(TAG, "알 수 없는 헤더: 0x%02X", data[0]);
        return;
    }

    // 길이 체크 (최소 2바이트: F1 + ChannelCount)
    if (len < 2) {
        T_LOGW(TAG, "패킷 길이 부족: %d", (int)len);
        return;
    }

    uint8_t ch_count = data[1];
    if (ch_count < 1 || ch_count > 20) {
        T_LOGW(TAG, "잘못된 채널 수: %d", ch_count);
        return;
    }

    // 데이터 길이 계산
    uint8_t expected_data_len = (ch_count + 3) / 4;
    size_t payload_len = len - 2;  // 헤더(2) 제외

    if (payload_len != expected_data_len) {
        T_LOGW(TAG, "데이터 길이 불일치: 예상 %d, 수신 %d", expected_data_len, (int)payload_len);
        return;
    }

    const uint8_t* payload = &data[2];

    // packed_data_t로 변환
    packed_data_t tally = {
        .data = const_cast<uint8_t*>(payload),
        .data_size = static_cast<uint8_t>(payload_len),
        .channel_count = ch_count  // 실제 채널 수 사용
    };

    if (!packed_data_is_valid(&tally)) {
        T_LOGW(TAG, "잘못된 Tally 데이터");
        return;
    }

    // 헥스 문자열 변환 (데이터만)
    char hex_str[16];
    packed_data_to_hex(&tally, hex_str, sizeof(hex_str));

    // Tally 상태 문자열 변환
    char tally_str[64];
    packed_data_format_tally(&tally, tally_str, sizeof(tally_str));

    T_LOGI(TAG, "LoRa 수신: [F1][%d][%s] (%d채널, %d바이트) → %s",
             ch_count, hex_str, ch_count, (int)payload_len, tally_str);

    // Tally 상태 변경 이벤트 발행
    tally_event_data_t event_data = {
        .source = SWITCHER_ROLE_PRIMARY,  // LoRa는 Primary로 간주
        .channel_count = ch_count,
        .tally_data = {0},
        .tally_value = packed_data_to_uint64(&tally)
    };
    memcpy(event_data.tally_data, payload, payload_len);

    event_bus_publish(EVT_TALLY_STATE_CHANGED, &event_data, sizeof(event_data));
    T_LOGI(TAG, "Tally 상태 변경 이벤트 발행");
}

// ============================================================================
// 버튼 롱프레스 콜백 - 카메라 ID 변경
// ============================================================================

static esp_err_t on_button_long_press(const event_data_t* event) {
    (void)event;  // 미사용

    // 현재 카메라 ID 읽기
    uint8_t current_id = config_service_get_camera_id();

    // 다음 ID 계산 (1-20 cycle)
    uint8_t new_id = current_id + 1;
    if (new_id > 20) {
        new_id = 1;
    }

    // 저장
    esp_err_t ret = config_service_set_camera_id(new_id);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "카메라 ID 변경: %d → %d", current_id, new_id);

        // 드라이버에도 적용
        led_service_set_camera_id(new_id);

        // 이벤트 발행
        event_bus_publish(EVT_CAMERA_ID_CHANGED, &new_id, sizeof(new_id));
    } else {
        T_LOGE(TAG, "카메라 ID 저장 실패: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

// ============================================================================
// 태그
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static const char* TAG = "tally_rx_app";
#pragma GCC diagnostic pop
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    tally_rx_config_t config;
    bool running;
    bool initialized;
} s_app = {
    .running = false,
    .initialized = false
};

// ============================================================================
// 앱 API
// ============================================================================

bool tally_rx_app_init(const tally_rx_config_t* config) {
    if (s_app.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return true;
    }

    T_LOGI(TAG, "Tally 수신 앱 초기화 중...");

    // ConfigService에서 RF 설정 가져오기
    config_device_t device_config;
    config_service_get_device(&device_config);

    // LoRa 초기화
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

    // 수신 콜백 등록
    lora_service_set_receive_callback(on_lora_receive);

    // WS2812 LED 초기화 (드라이버에서 PinConfig.h 사용, NVS에서 camera_id 로드)
    uint8_t camera_id = config_service_get_camera_id();
    esp_err_t led_ret = led_service_init(-1, 0, camera_id);
    if (led_ret == ESP_OK) {
        T_LOGI(TAG, "WS2812 초기화 완료 (카메라 ID: %d)", camera_id);
    } else {
        T_LOGW(TAG, "WS2812 초기화 실패: %s", esp_err_to_name(led_ret));
    }

    // 버튼 롱프레스 이벤트 구독 (카메라 ID 변경)
    event_bus_subscribe(EVT_BUTTON_LONG_PRESS, on_button_long_press);
    T_LOGI(TAG, "버튼 롱프레스 이벤트 구독 완료 (카메라 ID 변경)");

    // 버튼 서비스 초기화
    esp_err_t ret = button_service_init();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "버튼 서비스 초기화 실패: %s", esp_err_to_name(ret));
    }

    s_app.initialized = true;
    T_LOGI(TAG, "Tally 수신 앱 초기화 완료");

    // LoRa 설정 로그
    T_LOGI(TAG, "  주파수: %.1f MHz", s_app.config.frequency);
    T_LOGI(TAG, "  SF: %d, CR: 4/%d, BW: %.0f kHz",
             s_app.config.spreading_factor,
             s_app.config.coding_rate,
             s_app.config.bandwidth);
    T_LOGI(TAG, "  전력: %d dBm, SyncWord: 0x%02X",
             s_app.config.tx_power,
             s_app.config.sync_word);

    return true;
}

void tally_rx_app_start(void) {
    if (!s_app.initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return;
    }

    if (s_app.running) {
        T_LOGW(TAG, "이미 실행 중");
        return;
    }

    // LoRa 시작
    lora_service_start();

    // 버튼 서비스 시작
    button_service_start();
    T_LOGI(TAG, "버튼 서비스 시작");

    s_app.running = true;
    T_LOGI(TAG, "Tally 수신 앱 시작");
}

void tally_rx_app_stop(void) {
    if (!s_app.running) {
        return;
    }

    // 버튼 서비스 정지
    button_service_stop();

    // LoRa 정지
    lora_service_stop();

    s_app.running = false;
    T_LOGI(TAG, "Tally 수신 앱 정지");
}

void tally_rx_app_deinit(void) {
    tally_rx_app_stop();

    // WS2812 정리
    led_service_deinit();

    // LoRa 정리
    lora_service_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "Tally 수신 앱 정리 완료");
}

void tally_rx_app_loop(void) {
    // 수신은 콜백으로 처리되므로 별도 루프 처리 불필요
}

void tally_rx_app_print_status(void) {
    if (!s_app.initialized) {
        T_LOGI(TAG, "상태: 초기화되지 않음");
        return;
    }

    T_LOGI(TAG, "===== Tally 수신 앱 상태 =====");
    T_LOGI(TAG, "실행 중: %s", s_app.running ? "예" : "아니오");
    T_LOGI(TAG, "주파수: %.1f MHz", s_app.config.frequency);
    T_LOGI(TAG, "SF: %d, CR: 4/%d, BW: %.0f kHz",
             s_app.config.spreading_factor,
             s_app.config.coding_rate,
             s_app.config.bandwidth);
    T_LOGI(TAG, "==============================");
}

bool tally_rx_app_is_running(void) {
    return s_app.running;
}
