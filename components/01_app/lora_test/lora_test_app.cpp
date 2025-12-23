/**
 * @file lora_test_app.cpp
 * @brief LoRa 테스트 앱 구현
 *
 * 200ms마다 "Test" 메시지 송신
 * 10초마다 상태 출력
 */

#include "lora_test_app.h"
#include "LoRaService.h"
#include "LoRaConfig.h"
#include "event_bus.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "LoRaTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;
static TaskHandle_t s_task = nullptr;

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

// ============================================================================
// 태스크
// ============================================================================

/**
 * @brief LoRa 테스트 태스크
 * @note 200ms마다 "Test" 메시지 송신, 10초마다 상태 출력
 */
static void lora_test_task(void* param)
{
    ESP_LOGI(TAG, "LoRa 테스트 태스크 시작");

    uint32_t packet_count = 0;
    uint32_t last_status_time = xTaskGetTickCount();

    const char* test_msg = "Test";
    size_t msg_len = strlen(test_msg);

    while (s_running) {
        // 송신
        esp_err_t ret = lora_service_send((const uint8_t*)test_msg, msg_len);
        if (ret == ESP_OK) {
            packet_count++;
        } else if (ret == ESP_ERR_NOT_SUPPORTED) {
            // 송신 중 - 다음 시도에 재전송
            ESP_LOGW(TAG, "송신 중 - 패킷 대기");
        }

        // 10초마다 상태 출력
        uint32_t now = xTaskGetTickCount();
        if (now - last_status_time >= pdMS_TO_TICKS(10000)) {
            lora_service_status_t status = lora_service_get_status();
            ESP_LOGI(TAG, "상태: 송신=%u, 수신=%u, RSSI=%d",
                     status.packets_sent, status.packets_received, status.rssi);
            last_status_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "LoRa 테스트 태스크 종료");
    vTaskDelete(nullptr);
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

    // 태스크 생성
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        lora_test_task,
        "lora_test",
        4096,
        nullptr,
        configMAX_PRIORITIES - 4,
        &s_task,
        1
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "태스크 생성 실패");
        lora_service_stop();
        return ESP_FAIL;
    }

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

    // 태스크 종료 대기
    if (s_task) {
        int wait_count = 0;
        while (eTaskGetState(s_task) != eDeleted && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }
        s_task = nullptr;
    }

    lora_service_stop();
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 정지 완료");
}

void lora_test_app_deinit(void)
{
    lora_test_app_stop();
    lora_service_deinit();
    ESP_LOGI(TAG, "✓ LoRa 테스트 앱 해제 완료");
}

bool lora_test_app_is_running(void)
{
    return s_running;
}

} // extern "C"
