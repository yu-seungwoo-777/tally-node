/**
 * @file tally_rx_app.cpp
 * @brief Tally 수신 앱 구현
 */

#include "tally_rx_app.h"
#include "t_log.h"
#include "LoRaService.h"
#include "TallyTypes.h"
#include "LoRaConfig.h"
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

    // 헤더 분석 및 데이터 추출
    // 패킷 구조: [Header(1byte)][Data...]
    // Header: F1=8ch, F2=12ch, F3=16ch, F4=20ch
    const uint8_t* payload = data;
    size_t payload_len = len;

    if (len > 0) {
        uint8_t header = data[0];
        // F1-F4 헤더인 경우 제거
        if (header >= 0xF1 && header <= 0xF4) {
            payload = data + 1;
            payload_len = len - 1;
        }
    }

    // 길이 체크 (최대 255바이트, 255*4=1020채널)
    if (payload_len > 255) {
        T_LOGW(TAG, "페이로드 길이 초과: %d", (int)payload_len);
        return;
    }

    // packed_data_t로 변환 (명시적 캐스팅)
    packed_data_t tally = {
        .data = const_cast<uint8_t*>(payload),
        .data_size = static_cast<uint8_t>(payload_len),
        .channel_count = static_cast<uint8_t>(payload_len * 4)  // 1바이트 = 4채널 (2비트/채널)
    };

    if (!packed_data_is_valid(&tally)) {
        T_LOGW(TAG, "잘못된 Tally 데이터 (길이: %d)", (int)len);
        return;
    }

    // 헥스 문자열 변환
    char hex_str[16];
    packed_data_to_hex(&tally, hex_str, sizeof(hex_str));

    // Tally 상태 문자열 변환
    char tally_str[64];
    packed_data_format_tally(&tally, tally_str, sizeof(tally_str));

    T_LOGI(TAG, "LoRa 수신: [%s] (%d채널, %d바이트) → %s",
             hex_str, tally.channel_count, (int)payload_len, tally_str);
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
// 기본 설정 (LoRaConfig.h 사용)
// ============================================================================

const tally_rx_config_t TALLY_RX_DEFAULT_CONFIG = {
    .frequency = LORA_DEFAULT_FREQ,      // LoRaConfig.h 기본 주파수
    .spreading_factor = LORA_DEFAULT_SF, // LoRaConfig.h 기본 SF
    .coding_rate = LORA_DEFAULT_CR,      // LoRaConfig.h 기본 CR
    .bandwidth = LORA_DEFAULT_BW,        // LoRaConfig.h 기본 BW
    .tx_power = LORA_DEFAULT_TX_POWER,   // LoRaConfig.h 기본 전력
    .sync_word = LORA_DEFAULT_SYNC_WORD  // LoRaConfig.h 기본 SyncWord
};

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    tally_rx_config_t config;
    bool running;
    bool initialized;
} s_app = {
    .config = TALLY_RX_DEFAULT_CONFIG,
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

    // 설정 복사
    if (config) {
        s_app.config = *config;
    } else {
        s_app.config = TALLY_RX_DEFAULT_CONFIG;
    }

    // LoRa 초기화
    lora_service_config_t lora_config = {
        .frequency = s_app.config.frequency,
        .spreading_factor = s_app.config.spreading_factor,
        .coding_rate = s_app.config.coding_rate,
        .bandwidth = s_app.config.bandwidth,
        .tx_power = s_app.config.tx_power,
        .sync_word = s_app.config.sync_word
    };

    esp_err_t lora_ret = lora_service_init(&lora_config);
    if (lora_ret != ESP_OK) {
        T_LOGE(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
        return false;
    }

    // 수신 콜백 등록
    lora_service_set_receive_callback(on_lora_receive);

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

    s_app.running = true;
    T_LOGI(TAG, "Tally 수신 앱 시작");
}

void tally_rx_app_stop(void) {
    if (!s_app.running) {
        return;
    }

    // LoRa 정지
    lora_service_stop();

    s_app.running = false;
    T_LOGI(TAG, "Tally 수신 앱 정지");
}

void tally_rx_app_deinit(void) {
    tally_rx_app_stop();

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
