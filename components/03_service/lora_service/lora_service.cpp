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
#include "lora_protocol.h"
#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>

static const char* TAG __attribute__((unused)) = "LORA_SERVICE";
#ifdef DEVICE_MODE_TX
static const char* TAG_RF __attribute__((unused)) = "RF";
#endif

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

// 스캔 관련
static TaskHandle_t s_scan_task = nullptr;
static volatile bool s_scanning = false;
static volatile bool s_scan_stop_requested = false;
static float s_scan_start_freq = 0.0f;
static float s_scan_end_freq = 0.0f;
static float s_scan_step = 0.1f;
#define MAX_SCAN_CHANNELS 100

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief LoRa 스캔 시작 요청 이벤트 핸들러
 */
static esp_err_t on_lora_scan_start_request(const event_data_t* event) {
    if (event->type != EVT_LORA_SCAN_START) {
        return ESP_OK;
    }

    const auto* req = reinterpret_cast<const lora_scan_start_t*>(event->data);
    if (req == nullptr) {
        T_LOGW(TAG, "Invalid scan request (null data)");
        return ESP_ERR_INVALID_ARG;
    }

    return lora_service_start_scan(req->start_freq, req->end_freq, req->step);
}

/**
 * @brief LoRa 스캔 중지 요청 이벤트 핸들러
 */
static esp_err_t on_lora_scan_stop_request(const event_data_t* event) {
    if (event->type != EVT_LORA_SCAN_STOP) {
        return ESP_OK;
    }

    return lora_service_stop_scan();
}

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
 * @brief RF 변경 이벤트 핸들러 (TX/RX 공용)
 * RF 설정 변경 시 드라이버에 적용하고, TX는 broadcast 후 저장 요청
 */
static esp_err_t on_rf_changed(const event_data_t* event) {
    if (event->type != EVT_RF_CHANGED) {
        return ESP_OK;
    }

    const auto* rf = reinterpret_cast<const lora_rf_event_t*>(event->data);
    if (rf == nullptr) {
        T_LOGW(TAG, "rf 데이터가 NULL");
        return ESP_OK;
    }

#ifdef DEVICE_MODE_TX
    T_LOGI(TAG_RF, "드라이버 적용: %.1f MHz, Sync 0x%02X", rf->frequency, rf->sync_word);
#else
    T_LOGI(TAG, "드라이버 적용: %.1f MHz, Sync 0x%02X", rf->frequency, rf->sync_word);
#endif

    // 1. 드라이버에 RF 설정 적용
    lora_driver_set_frequency(rf->frequency);
    lora_driver_set_sync_word(rf->sync_word);

#ifdef DEVICE_MODE_TX
    // 2. TX: RF 변경 broadcast 10회 (RX들에게 알림, 5초간)
    // 패킷 구조: [0xE3][frequency(4바이트 float)][sync_word(1바이트)]
    uint8_t broadcast_pkt[6];
    broadcast_pkt[0] = 0xE3;  // RF 설정 broadcast 헤더
    memcpy(&broadcast_pkt[1], &rf->frequency, sizeof(float));
    broadcast_pkt[5] = rf->sync_word;

    for (int i = 0; i < 10; i++) {
        lora_service_send(broadcast_pkt, sizeof(broadcast_pkt));
        vTaskDelay(pdMS_TO_TICKS(500));  // 500ms 간격
    }

    T_LOGI(TAG_RF, "RF broadcast 완료: %.1f MHz, Sync 0x%02X (10회)", rf->frequency, rf->sync_word);

    // 3. NVS 저장 요청 (broadcast 완료 후)
    event_bus_publish(EVT_RF_SAVED, rf, sizeof(*rf));
#endif

    return ESP_OK;
}

// ============================================================================
// 패킷 분류 및 처리
// ============================================================================

/**
 * @brief Tally 패킷 처리 (0xF1~0xF4)
 *
 * TX/RX 공통으로 수신된 Tally 패킷을 파싱하고 이벤트를 발행합니다.
 */
static void process_tally_packet(const uint8_t* data, size_t length, int16_t rssi, float snr)
{
    // 패킷 구조: [F1-F4][ChannelCount][Data...]
    if (length < 2) {
        T_LOGW(TAG, "Tally 패킷 길이 부족: %d", (int)length);
        return;
    }

    uint8_t ch_count = data[1];
    if (ch_count < 1 || ch_count > 20) {
        T_LOGW(TAG, "잘못된 채널 수: %d", ch_count);
        return;
    }

    // 데이터 길이 계산
    uint8_t expected_data_len = (ch_count + 3) / 4;
    size_t payload_len = length - 2;  // 헤더(2) 제외

    if (payload_len != expected_data_len || payload_len > 8) {
        T_LOGW(TAG, "Tally 데이터 길이 불일치: 예상 %d, 수신 %d", expected_data_len, (int)payload_len);
        return;
    }

    const uint8_t* payload = &data[2];

    // packed_data_t 변환
    packed_data_t tally = {
        .data = const_cast<uint8_t*>(payload),
        .data_size = static_cast<uint8_t>(payload_len),
        .channel_count = ch_count
    };

    if (!packed_data_is_valid(&tally)) {
        T_LOGW(TAG, "잘못된 Tally 데이터");
        return;
    }

    // Tally 상태 이벤트 발행
    static tally_event_data_t s_tally_event;  // 정적 변수 (이벤트 발행 후에도 유효)
    s_tally_event.source = SWITCHER_ROLE_PRIMARY;
    s_tally_event.channel_count = ch_count;
    s_tally_event.tally_value = packed_data_to_uint64(&tally);
    memcpy(s_tally_event.tally_data, payload, payload_len);

    event_bus_publish(EVT_TALLY_STATE_CHANGED, &s_tally_event, sizeof(s_tally_event));

    // 로그 출력
    char hex_str[16];
    packed_data_to_hex(&tally, hex_str, sizeof(hex_str));

    char tally_str[64];
    packed_data_format_tally(&tally, tally_str, sizeof(tally_str));

    T_LOGI(TAG, "Tally: [F1][%d][%s] → %s (RSSI:%d SNR:%.1f)",
             ch_count, hex_str, tally_str, rssi, snr);
}

/**
 * @brief 드라이버 수신 콜백 (내부)
 *
 * 패킷 헤더를 기준으로 분류하여 처리합니다.
 */
static void on_driver_receive(const uint8_t* data, size_t length, int16_t rssi, float snr)
{
    s_packets_received++;

    // 길이 제한
    if (length > MAX_PACKET_SIZE) {
        length = MAX_PACKET_SIZE;
    }

    // 패킷이 비어있으면 무시
    if (length == 0 || data == nullptr) {
        return;
    }

    // 헤더 기반 분류
    uint8_t header = data[0];

    // Tally 데이터 (0xF1~0xF4)
    if (LORA_IS_TALLY_HEADER(header)) {
        process_tally_packet(data, length, rssi, snr);
    }
    // TX→RX 명령 (0xE0~0xEF) - 추후 구현
    else if (LORA_IS_TX_COMMAND_HEADER(header)) {
        T_LOGD(TAG, "TX 명령 수신 (추후 구현): 0x%02X", header);
    }
    // RX→TX 응답 (0xD0~0xDF) - 추후 구현
    else if (LORA_IS_RX_RESPONSE_HEADER(header)) {
        T_LOGD(TAG, "RX 응답 수신 (추후 구현): 0x%02X", header);
    }
    // 알 수 없는 헤더
    else {
        T_LOGD(TAG, "알 수 없는 패킷: 0x%02X (%d bytes)", header, (int)length);
    }

    // RSSI/SNR 이벤트 발행
    lora_status_t driver_status = lora_driver_get_status();
    lora_rssi_event_t rssi_event;
    rssi_event.is_running = s_running;
    rssi_event.is_initialized = s_initialized;
    rssi_event.chip_type = (uint8_t)driver_status.chip_type;
    rssi_event.frequency = driver_status.frequency;
    rssi_event.rssi = rssi;
    rssi_event.snr = (int8_t)snr;
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

    // 스캔 이벤트 구독
    event_bus_subscribe(EVT_LORA_SCAN_START, on_lora_scan_start_request);
    event_bus_subscribe(EVT_LORA_SCAN_STOP, on_lora_scan_stop_request);

    // RF 변경 이벤트 구독 (TX/RX 공용)
    event_bus_subscribe(EVT_RF_CHANGED, on_rf_changed);

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

    // RSSI 이벤트 발행 (칩 타입 전달을 위해 시작 직후 발행)
    lora_status_t driver_status = lora_driver_get_status();
    lora_rssi_event_t rssi_event = {
        .is_running = s_running,
        .is_initialized = s_initialized,
        .chip_type = (uint8_t)driver_status.chip_type,
        .frequency = driver_status.frequency,
        .rssi = driver_status.rssi,
        .snr = driver_status.snr
    };
    event_bus_publish(EVT_LORA_RSSI_CHANGED, &rssi_event, sizeof(rssi_event));

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

    // 스캔 이벤트 구독 취소
    event_bus_unsubscribe(EVT_LORA_SCAN_START, on_lora_scan_start_request);
    event_bus_unsubscribe(EVT_LORA_SCAN_STOP, on_lora_scan_stop_request);

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

// ============================================================================
// 주파수 스캔 태스크
// ============================================================================

/**
 * @brief 주파수 스캔 태스크
 *
 * - 각 채널에서 RSSI 측정
 * - 진행 상황을 이벤트로 발행
 * - 완료 시 전체 결과 발행
 */
static void lora_scan_task(void* arg)
{
    T_LOGI(TAG, "스캔 태스크 시작: %.1f ~ %.1f MHz (step=%.1f)",
           s_scan_start_freq, s_scan_end_freq, s_scan_step);

    // 예상 채널 수 계산
    int total_channels = (int)((s_scan_end_freq - s_scan_start_freq) / s_scan_step) + 1;
    if (total_channels > MAX_SCAN_CHANNELS) {
        total_channels = MAX_SCAN_CHANNELS;
    }

    lora_channel_info_t results[MAX_SCAN_CHANNELS];
    size_t result_count = 0;

    // 채널별 스캔 (진행도 발행을 위해)
    for (float freq = s_scan_start_freq; freq <= s_scan_end_freq && result_count < MAX_SCAN_CHANNELS; freq += s_scan_step) {
        // 중지 요청 확인
        if (s_scan_stop_requested) {
            T_LOGI(TAG, "스캔 중지 요청됨");
            break;
        }

        // 단일 채널 스캔 (드라이버에는 임시 버퍼 사용 후 타입 변환)
        channel_info_t driver_result;
        size_t count = 0;
        esp_err_t ret = lora_driver_scan_channels(freq, freq + 1.0f, 1.0f, &driver_result, 1, &count);

        if (ret == ESP_OK && count > 0) {
            // 드라이버 타입 → 이벤트 타입 변환
            results[result_count].frequency = driver_result.frequency;
            results[result_count].rssi = driver_result.rssi;
            results[result_count].noise_floor = driver_result.noise_floor;
            results[result_count].clear_channel = driver_result.clear_channel;
            result_count++;

            // 진행률 계산
            uint8_t progress = (uint8_t)((result_count * 100) / total_channels);

            // 진행 이벤트 발행
            lora_scan_progress_t progress_event = {
                .progress = progress,
                .current_freq = freq,
                .result = {
                    .frequency = results[result_count - 1].frequency,
                    .rssi = results[result_count - 1].rssi,
                    .noise_floor = results[result_count - 1].noise_floor,
                    .clear_channel = results[result_count - 1].clear_channel
                }
            };
            event_bus_publish(EVT_LORA_SCAN_PROGRESS, &progress_event, sizeof(progress_event));

            T_LOGD(TAG, "스캔: %.1f MHz, RSSI %d dBm (%d%%)",
                   freq, results[result_count - 1].rssi, progress);
        }

        // 채널 간 약간 대기 (진행 이벤트가 전달될 시간 확보)
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // 스캔 완료 이벤트 발행
    lora_scan_complete_t complete_event = {};
    complete_event.count = (result_count > 100) ? 100 : (uint8_t)result_count;
    memcpy(complete_event.channels, results, sizeof(lora_channel_info_t) * complete_event.count);

    event_bus_publish(EVT_LORA_SCAN_COMPLETE, &complete_event, sizeof(complete_event));

    T_LOGI(TAG, "스캔 완료: %d개 채널", result_count);

    // 상태 정리
    s_scanning = false;
    s_scan_stop_requested = false;
    s_scan_task = nullptr;

    // 수신 모드 복귀
    lora_driver_start_receive();

    vTaskDelete(nullptr);
}

// ============================================================================
// 스캔 공개 API
// ============================================================================

esp_err_t lora_service_start_scan(float start_freq, float end_freq, float step)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_scanning) {
        T_LOGW(TAG, "이미 스캔 중");
        return ESP_ERR_INVALID_STATE;
    }

    if (start_freq >= end_freq || step <= 0.0f) {
        T_LOGE(TAG, "잘못된 스캔 파라미터");
        return ESP_ERR_INVALID_ARG;
    }

    // 파라미터 저장
    s_scan_start_freq = start_freq;
    s_scan_end_freq = end_freq;
    s_scan_step = step;
    s_scan_stop_requested = false;
    s_scanning = true;

    // 스캔 태스크 생성 (스택 8KB - 100채널 배열 + RadioLib 사용)
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        lora_scan_task,
        "lora_scan_task",
        8192,
        nullptr,
        5,  // 우선순위 (낮음 - 긴 스캔 시간)
        &s_scan_task,
        1
    );

    if (task_ret != pdPASS) {
        T_LOGE(TAG, "스캔 태스크 생성 실패");
        s_scanning = false;
        return ESP_FAIL;
    }

    T_LOGI(TAG, "스캔 시작됨");
    return ESP_OK;
}

esp_err_t lora_service_stop_scan(void)
{
    if (!s_scanning) {
        return ESP_ERR_INVALID_STATE;
    }

    s_scan_stop_requested = true;
    T_LOGI(TAG, "스캔 중지 요청됨");

    return ESP_OK;
}

bool lora_service_is_scanning(void)
{
    return s_scanning;
}

} // extern "C"
