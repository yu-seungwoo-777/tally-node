/**
 * @file LoRaService.cpp
 * @brief LoRa Service 구현
 *
 * 송신 큐 기반 아키텍처:
 * - lora_service_send → 큐에 패킷 추가
 * - tx_task → 큐에서 패킷 가져와서 송신
 */

#include "lora_service.h"
#include "lora_driver.h"
#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>

static const char* TAG __attribute__((unused)) = "LORA_SERVICE";

// ============================================================================
// 상수
// ============================================================================

#define TX_QUEUE_SIZE 8          // 송신 큐 크기
#define MAX_PACKET_SIZE 256      // 최대 패킷 크기

// ============================================================================
// 송신 패킷 구조체
// ============================================================================

typedef struct {
    uint8_t data[MAX_PACKET_SIZE];
    size_t length;
} lora_tx_packet_t;

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_initialized = false;
static bool s_running = false;
static lora_service_receive_callback_t s_user_callback = nullptr;

// 통계
static uint32_t s_packets_sent = 0;
static uint32_t s_packets_received = 0;
static uint32_t s_tx_dropped = 0;          // 큐 오버플로우로 폐기된 패킷

// 큐 및 태스크
static QueueHandle_t s_tx_queue = nullptr;
static TaskHandle_t s_tx_task = nullptr;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief LoRa 송신 요청 이벤트 핸들러
 */
static esp_err_t on_lora_send_request(const event_data_t* event) {
    if (event->type != EVT_LORA_SEND_REQUEST) {
        return ESP_OK;
    }

    const auto* req = reinterpret_cast<const lora_send_request_t*>(event->data);
    if (req == nullptr || req->data == nullptr) {
        T_LOGW(TAG, "Invalid send request (null data)");
        return ESP_ERR_INVALID_ARG;
    }

    return lora_service_send(req->data, req->length);
}

/**
 * @brief 드라이버 수신 콜백 (내부)
 */
static void on_driver_receive(const uint8_t* data, size_t length, int16_t rssi, float snr)
{
    s_packets_received++;

    // 이벤트 발행 (패킷 데이터 + RSSI/SNR)
    lora_packet_event_t packet_event = {};
    if (length > MAX_PACKET_SIZE) {
        length = MAX_PACKET_SIZE;
    }
    memcpy(packet_event.data, data, length);
    packet_event.length = length;
    packet_event.rssi = rssi;
    packet_event.snr = snr;

    event_bus_publish(EVT_LORA_PACKET_RECEIVED, &packet_event, sizeof(packet_event));

    // RSSI/SNR 이벤트도 함께 발행 (패킷 수신 시마다 즉시 갱신)
    lora_rssi_event_t rssi_event = {
        .is_running = s_running,
        .is_initialized = s_initialized,
        .chip_type = 0,  // 드라이버에서 필요시 읽기
        .frequency = 0.0f,
        .rssi = rssi,
        .snr = (int8_t)snr
    };
    event_bus_publish(EVT_LORA_RSSI_CHANGED, &rssi_event, sizeof(rssi_event));

    // 사용자 콜백 호출 (레거시 지원)
    if (s_user_callback) {
        s_user_callback(data, length);
    }
}

/**
 * @brief 송신 큐 태스크
 *
 * - 블로킹 없이 주기적으로 큐 체크
 * - 큐에 패킷이 있으면 즉시 송신 → 수신 복귀
 * - 큐가 비어있으면 계속 수신 모드 유지
 */
static void lora_txq_task(void* arg)
{
    T_LOGI(TAG, "LoRa 송신 큐 태스크 시작");

    lora_tx_packet_t packet;

    while (s_running) {
        // 비블로킹으로 큐 체크 (패킷 있으면 즉시 송신)
        if (xQueueReceive(s_tx_queue, &packet, 0) == pdTRUE) {
            // 드라이버가 송신 중이면 대기
            while (s_running && lora_driver_is_transmitting()) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            if (!s_running) {
                break;
            }

            // 송신 (완료 후 드라이버가 자동으로 수신 모드로 전환)
            esp_err_t ret = lora_driver_transmit(packet.data, packet.length);

            if (ret == ESP_OK) {
                s_packets_sent++;
                T_LOGV(TAG, "송신: %zu bytes", packet.length);
                event_bus_publish(EVT_LORA_PACKET_SENT, &s_packets_sent, sizeof(s_packets_sent));
            } else {
                T_LOGI(TAG, "송신 실패: %d", ret);
            }
        }

        // 큐가 비어있으면 짧게 대기 후 다시 체크 (수신 모드 유지)
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    T_LOGI(TAG, "송신 태스크 종료");
    vTaskDelete(nullptr);
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

esp_err_t lora_service_init(const lora_service_config_t* config)
{
    if (s_initialized) {
        T_LOGI(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "LoRa Service 초기화 중...");

    // 송신 큐 생성
    s_tx_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(lora_tx_packet_t));
    if (s_tx_queue == nullptr) {
        T_LOGI(TAG, "송신 큐 생성 실패");
        return ESP_FAIL;
    }

    // 드라이버 설정 구성
    lora_config_t driver_config;

    if (config == nullptr) {
        T_LOGE(TAG, "config는 nullptr일 수 없습니다");
        vQueueDelete(s_tx_queue);
        s_tx_queue = nullptr;
        return ESP_ERR_INVALID_ARG;
    }

    driver_config.frequency = config->frequency;
    driver_config.spreading_factor = config->spreading_factor;
    driver_config.coding_rate = config->coding_rate;
    driver_config.bandwidth = config->bandwidth;
    driver_config.tx_power = config->tx_power;
    driver_config.sync_word = config->sync_word;

    // 드라이버 초기화
    esp_err_t ret = lora_driver_init(&driver_config);
    if (ret != ESP_OK) {
        T_LOGI(TAG, "드라이버 초기화 실패");
        vQueueDelete(s_tx_queue);
        s_tx_queue = nullptr;
        return ESP_FAIL;
    }

    // 드라이버 수신 콜백 등록
    lora_driver_set_receive_callback(on_driver_receive);

    s_initialized = true;
    T_LOGI(TAG, "LoRa Service 초기화 완료 (큐 크기: %d)", TX_QUEUE_SIZE);

    // 이벤트 발행
    bool running = false;
    event_bus_publish(EVT_LORA_STATUS_CHANGED, &running, sizeof(running));

    return ESP_OK;
}

esp_err_t lora_service_start(void)
{
    if (!s_initialized) {
        T_LOGI(TAG, "초기화되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        T_LOGI(TAG, "이미 실행 중");
        return ESP_OK;
    }

    T_LOGI(TAG, "LoRa Service 시작 중...");

    // 송신 요청 이벤트 구독
    esp_err_t ret = event_bus_subscribe(EVT_LORA_SEND_REQUEST, on_lora_send_request);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "송신 요청 이벤트 구독 실패");
        return ret;
    }

    // 수신 모드 시작
    ret = lora_driver_start_receive();
    if (ret != ESP_OK) {
        T_LOGI(TAG, "수신 모드 시작 실패");
        event_bus_unsubscribe(EVT_LORA_SEND_REQUEST, on_lora_send_request);
        return ret;
    }

    // 송신 큐 태스크 생성
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        lora_txq_task,
        "lora_txq_task",
        4096,
        nullptr,
        6,  // 우선순위 (중간)
        &s_tx_task,
        1
    );

    if (task_ret != pdPASS) {
        T_LOGI(TAG, "송신 태스크 생성 실패");
        event_bus_unsubscribe(EVT_LORA_SEND_REQUEST, on_lora_send_request);
        return ESP_FAIL;
    }

    s_running = true;
    T_LOGI(TAG, "LoRa Service 시작 완료");

    // 이벤트 발행
    bool running = true;
    event_bus_publish(EVT_LORA_STATUS_CHANGED, &running, sizeof(running));

    return ESP_OK;
}

void lora_service_stop(void)
{
    if (!s_running) {
        return;
    }

    T_LOGI(TAG, "LoRa Service 정지 중...");
    s_running = false;

    // 송신 요청 이벤트 구독 취소
    event_bus_unsubscribe(EVT_LORA_SEND_REQUEST, on_lora_send_request);

    // 태스크 종료 대기
    if (s_tx_task) {
        int wait_count = 0;
        while (eTaskGetState(s_tx_task) != eDeleted && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }
        s_tx_task = nullptr;
    }

    T_LOGI(TAG, "LoRa Service 정지 완료");

    // 이벤트 발행
    bool running = false;
    event_bus_publish(EVT_LORA_STATUS_CHANGED, &running, sizeof(running));
}

void lora_service_deinit(void)
{
    lora_service_stop();

    if (s_tx_queue) {
        vQueueDelete(s_tx_queue);
        s_tx_queue = nullptr;
    }

    lora_driver_deinit();
    s_initialized = false;
    T_LOGI(TAG, "LoRa Service 해제 완료");
}

esp_err_t lora_service_send(const uint8_t* data, size_t length)
{
    if (!s_initialized || !s_tx_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    if (length > MAX_PACKET_SIZE) {
        T_LOGI(TAG, "패킷 크기 초과: %zu > %d", length, MAX_PACKET_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    lora_tx_packet_t packet = {
        .data = {0},
        .length = length
    };
    memcpy(packet.data, data, length);

    // 비블로킹 모드로 큐에 추가
    if (xQueueSend(s_tx_queue, &packet, 0) == pdTRUE) {
        return ESP_OK;
    }

    s_tx_dropped++;
    T_LOGI(TAG, "송신 큐 full (패킷 폐기)");
    return ESP_ERR_NO_MEM;
}

esp_err_t lora_service_send_string(const char* str)
{
    if (str == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return lora_service_send((const uint8_t*)str, strlen(str));
}

// ============================================================================
// Tally 패킷 헬퍼 함수
// ============================================================================
// 패킷 구조: [F1][ChannelCount][Data...]
// - F1: 고정 헤더
// - ChannelCount: 실제 채널 수 (1-20)
// - Data: packed tally 데이터
// ============================================================================

esp_err_t lora_service_send_tally(const packed_data_t* tally)
{
    if (tally == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!packed_data_is_valid(tally)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 패킷 버퍼: [F1][ChannelCount][Data...]
    uint8_t packet[16];  // F1(1) + ChCount(1) + 최대 데이터(5) = 7바이트

    packet[0] = 0xF1;  // 고정 헤더
    packet[1] = tally->channel_count;  // 채널 수 (1-20)

    for (uint8_t i = 0; i < tally->data_size && i < 14; i++) {
        packet[2 + i] = tally->data[i];
    }

    size_t packet_size = 2 + tally->data_size;
    return lora_service_send(packet, packet_size);
}

void lora_service_set_receive_callback(lora_service_receive_callback_t callback) {
    s_user_callback = callback;
}

lora_service_status_t lora_service_get_status(void)
{
    lora_service_status_t status = {
        .is_running = s_running,
        .is_initialized = s_initialized,
        .chip_type = LORA_SERVICE_CHIP_UNKNOWN,
        .frequency = 0.0f,
        .rssi = -120,
        .snr = 0,
        .packets_sent = s_packets_sent,
        .packets_received = s_packets_received,
    };

    if (s_initialized) {
        lora_status_t driver_status = lora_driver_get_status();
        status.chip_type = (lora_service_chip_type_t)driver_status.chip_type;
        status.frequency = driver_status.frequency;
        status.rssi = driver_status.rssi;
        status.snr = driver_status.snr;
    }

    return status;
}

bool lora_service_is_running(void)
{
    return s_running;
}

bool lora_service_is_initialized(void)
{
    return s_initialized;
}

esp_err_t lora_service_set_frequency(float freq_mhz)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return lora_driver_set_frequency(freq_mhz);
}

esp_err_t lora_service_set_sync_word(uint8_t sync_word)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return lora_driver_set_sync_word(sync_word);
}

} // extern "C"
