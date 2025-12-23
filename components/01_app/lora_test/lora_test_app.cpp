/**
 * @file lora_test_app.cpp
 * @brief LoRa 테스트 앱 구현
 *
 * 버튼으로 제어:
 * - 단일 클릭: "Test" 메시지 송신
 * - 롱 프레스: 현재 통계 출력
 */

#include "lora_test_app.h"
#include "LoRaService.h"
#include "LoRaConfig.h"
#include "event_bus.h"
#include "button_poll.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "LoRaTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief LoRa 상태 변경 이벤트 핸들러
 */
static esp_err_t on_lora_status_changed(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(bool)) {
        bool running = *(const bool*)event->data;
        ESP_LOGI(TAG, "이벤트: LoRa 상태 변경 -> %s", running ? "실행 중" : "정지");
    }
    return ESP_OK;
}

/**
 * @brief LoRa 패킷 수신 이벤트 핸들러
 */
static esp_err_t on_lora_packet_received(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(uint32_t)) {
        uint32_t length = *(const uint32_t*)event->data;
        ESP_LOGI(TAG, "이벤트: LoRa 패킷 수신 (%u bytes)", length);
    }
    return ESP_OK;
}

/**
 * @brief LoRa 패킷 송신 이벤트 핸들러
 */
static esp_err_t on_lora_packet_sent(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(uint32_t)) {
        uint32_t count = *(const uint32_t*)event->data;
        ESP_LOGI(TAG, "이벤트: LoRa 패킷 송신 (총 %u)", count);
    }
    return ESP_OK;
}

/**
 * @brief LoRa 수신 콜백
 */
static void on_lora_received(const uint8_t* data, size_t length)
{
    // 수신 시 상태 확인
    lora_service_status_t status = lora_service_get_status();
    ESP_LOGI(TAG, "수신: %.*s | 송신=%u, 수신=%u, RSSI=%d",
             length, data,
             status.packets_sent,
             status.packets_received,
             status.rssi);
}

/**
 * @brief 버튼 이벤트 핸들러
 *
 * - 단일 클릭: "Test" 메시지 송신
 * - 롱 프레스: 현재 통계 출력
 */
static void on_button_event(button_action_t action)
{
    switch (action) {
        case BUTTON_ACTION_SINGLE:
            // 단일 클릭 - 메시지 송신
            {
                const char* test_msg = "Test";
                size_t msg_len = strlen(test_msg);
                esp_err_t ret = lora_service_send((const uint8_t*)test_msg, msg_len);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[버튼] 송신: Test");
                } else if (ret == ESP_ERR_NO_MEM) {
                    ESP_LOGW(TAG, "[버튼] 송신 큐 full");
                } else {
                    ESP_LOGW(TAG, "[버튼] 송신 실패: %d", ret);
                }
            }
            break;

        case BUTTON_ACTION_LONG:
            // 롱 프레스 - 통계 출력
            {
                lora_service_status_t status = lora_service_get_status();
                ESP_LOGI(TAG, "[버튼] 롱 프레스 - 통계:");
                ESP_LOGI(TAG, "  송신: %u", status.packets_sent);
                ESP_LOGI(TAG, "  수신: %u", status.packets_received);
                ESP_LOGI(TAG, "  RSSI: %d dBm", status.rssi);
                ESP_LOGI(TAG, "  SNR: %d dB", status.snr);
            }
            break;

        case BUTTON_ACTION_LONG_RELEASE:
            ESP_LOGI(TAG, "[버튼] 롱 프레스 해제");
            break;
    }
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t lora_test_app_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LoRa 테스트 앱 초기화");
    ESP_LOGI(TAG, "========================================");

    // Event Bus 초기화
    ESP_LOGI(TAG, "Event Bus 초기화 중...");
    esp_err_t ret = event_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event Bus 초기화 실패");
        return ret;
    }

    // LoRa 이벤트 구독
    event_bus_subscribe(EVT_LORA_STATUS_CHANGED, on_lora_status_changed);
    event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    event_bus_subscribe(EVT_LORA_PACKET_SENT, on_lora_packet_sent);
    ESP_LOGI(TAG, "LoRa 이벤트 구독 완료");

    // LoRa Service 초기화
    ESP_LOGI(TAG, "LoRa Service 초기화 중...");

    lora_service_config_t config = {
        .frequency = LORA_DEFAULT_FREQ,
        .spreading_factor = LORA_DEFAULT_SF,
        .coding_rate = LORA_DEFAULT_CR,
        .bandwidth = LORA_DEFAULT_BW,
        .tx_power = LORA_DEFAULT_TX_POWER,
        .sync_word = LORA_DEFAULT_SYNC_WORD,
    };

    ret = lora_service_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LoRa Service 초기화 실패");
        return ret;
    }

    // 수신 콜백 등록
    lora_service_set_receive_callback(on_lora_received);

    // 버튼 폴링 초기화
    ESP_LOGI(TAG, "버튼 폴링 초기화 중...");
    ret = button_poll_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "버튼 폴링 초기화 실패");
        return ret;
    }
    button_poll_set_callback(on_button_event);
    ESP_LOGI(TAG, "버튼 폴링 초기화 완료");

    ESP_LOGI(TAG, "단일 클릭: 송신 | 롱 프레스: 통계");
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 초기화 완료");
    return ESP_OK;
}

esp_err_t lora_test_app_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "LoRa 테스트 앱 시작 중...");

    // LoRa Service 시작
    esp_err_t ret = lora_service_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LoRa Service 시작 실패");
        return ret;
    }

    // 버튼 폴링 시작
    button_poll_start();

    s_running = true;
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 시작 완료 (%.0f MHz)", LORA_DEFAULT_FREQ);

    // 테스트: 이벤트 발생
    ESP_LOGI(TAG, "테스트: EVT_SYSTEM_READY 이벤트 발행");
    event_bus_publish(EVT_SYSTEM_READY, nullptr, 0);

    return ESP_OK;
}

void lora_test_app_stop(void)
{
    if (!s_running) {
        return;
    }

    ESP_LOGI(TAG, "LoRa 테스트 앱 정지 중...");
    s_running = false;

    button_poll_stop();
    lora_service_stop();
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 정지 완료");
}

void lora_test_app_deinit(void)
{
    lora_test_app_stop();
    button_poll_deinit();
    lora_service_deinit();
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 해제 완료");
}

bool lora_test_app_is_running(void)
{
    return s_running;
}

} // extern "C"
