/**
 * @file device_manager.cpp
 * @brief Device Manager 구현 (이벤트 기반)
 *
 * TX: 주기적 상태 요청, RX 디바이스 리스트 관리
 * RX: 상태 요청 수신 시 응답 송신
 *
 * 의존성: lora_service, event_bus만 직접 사용
 *         나머지는 이벤트로 데이터 수신
 */

#include "device_manager.h"
#include "lora_service.h"
#include "lora_protocol.h"
#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "DeviceMgr";

// ============================================================================
// 공통 상태
// ============================================================================

static struct {
    bool initialized;
    bool running;
} s_mgr = {
    .initialized = false,
    .running = false
};

// ============================================================================
// TX 전용 (주기적 상태 요청, 디바이스 리스트 관리)
// ============================================================================

#ifdef DEVICE_MODE_TX

#define STATUS_REQUEST_INTERVAL_MS  30000  // 기본 30초
#define MAX_DEVICES  20

static struct {
    uint32_t request_interval_ms;
    uint32_t last_request_time;
    device_info_t devices[MAX_DEVICES];
    uint8_t device_count;
} s_tx = {
    .request_interval_ms = STATUS_REQUEST_INTERVAL_MS,
    .last_request_time = 0,
    .device_count = 0
};

/**
 * @brief 상태 요청 송신
 */
static esp_err_t send_status_request(void)
{
    // 패킷: [E0] (헤더만, broadcast)
    uint8_t packet[] = { LORA_HDR_STATUS_REQ };

    esp_err_t ret = lora_service_send(packet, sizeof(packet));
    if (ret == ESP_OK) {
        s_tx.last_request_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        T_LOGI(TAG, "상태 요청 송신 (Broadcast)");
    } else {
        T_LOGE(TAG, "상태 요청 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief 상태 응답 수신 처리 (0xD0)
 */
static void on_status_response(const lora_msg_status_t* status, int16_t rssi, float snr)
{
    char device_id_str[5];
    lora_device_id_to_str(status->device_id, device_id_str);

    T_LOGI(TAG, "상태 수신: ID=%s Bat=%d%% Cam=%d Up=%us Freq=%.1f SW=0x%02X (RSSI:%d SNR:%.1f)",
             device_id_str, status->battery, status->camera_id, status->uptime,
             status->frequency, status->sync_word, rssi, snr);

    // 디바이스 리스트 업데이트
    uint32_t now = xTaskGetTickCount();

    // 기존 디바이스 찾기
    int found_idx = -1;
    for (uint8_t i = 0; i < s_tx.device_count; i++) {
        if (lora_device_id_equals(s_tx.devices[i].device_id, status->device_id)) {
            found_idx = i;
            break;
        }
    }

    // 새 디바이스 추가 또는 기존 디바이스 업데이트
    if (found_idx < 0) {
        // 새 디바이스 추가
        if (s_tx.device_count < MAX_DEVICES) {
            found_idx = s_tx.device_count++;
            memcpy(s_tx.devices[found_idx].device_id, status->device_id, 4);
        } else {
            T_LOGW(TAG, "디바이스 리스트 꽉참 (%d)", MAX_DEVICES);
            return;
        }
    }

    // 디바이스 정보 업데이트
    s_tx.devices[found_idx].last_rssi = rssi;
    s_tx.devices[found_idx].last_snr = (int8_t)snr;
    s_tx.devices[found_idx].battery = status->battery;
    s_tx.devices[found_idx].camera_id = status->camera_id;
    s_tx.devices[found_idx].uptime = status->uptime;
    s_tx.devices[found_idx].brightness = status->brightness;
    s_tx.devices[found_idx].is_stopped = (status->flags & LORA_STATUS_FLAG_STOPPED) != 0;
    s_tx.devices[found_idx].last_seen = now;
    s_tx.devices[found_idx].frequency = status->frequency;
    s_tx.devices[found_idx].sync_word = status->sync_word;

    // 디바이스 리스트 변경 이벤트 발행
    device_list_event_t list_event;
    memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * s_tx.device_count);
    list_event.count = s_tx.device_count;
    list_event.registered_count = s_tx.device_count;
    event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
}

/**
 * @brief RX 응답 이벤트 핸들러 (0xD0~0xDF)
 */
static esp_err_t on_lora_rx_response(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;
    const uint8_t* data = packet->data;
    size_t len = packet->length;

    if (len == 0) {
        return ESP_OK;
    }

    uint8_t header = data[0];

    // 상태 응답 (0xD0)
    if (header == LORA_HDR_STATUS) {
        if (len < sizeof(lora_msg_status_t)) {
            T_LOGW(TAG, "상태 응답 길이 부족: %d", (int)len);
            return ESP_OK;
        }

        const lora_msg_status_t* status = (const lora_msg_status_t*)data;
        on_status_response(status, packet->rssi, packet->snr);
    }
    // ACK 응답 (0xD1)
    else if (header == LORA_HDR_ACK) {
        if (len >= sizeof(lora_msg_ack_t)) {
            const lora_msg_ack_t* ack = (const lora_msg_ack_t*)data;
            char device_id_str[5];
            lora_device_id_to_str(ack->device_id, device_id_str);
            T_LOGI(TAG, "ACK 수신: ID=%s Cmd=0x%02X Result=%d",
                     device_id_str, ack->cmd_header, ack->result);
        }
    }
    // PONG 응답 (0xD2)
    else if (header == LORA_HDR_PONG) {
        if (len >= sizeof(lora_msg_pong_t)) {
            const lora_msg_pong_t* pong = (const lora_msg_pong_t*)data;
            char device_id_str[5];
            lora_device_id_to_str(pong->device_id, device_id_str);
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint16_t ping_ms = (now & 0xFFFF) - pong->tx_timestamp_low;
            T_LOGI(TAG, "PONG 수신: ID=%s Ping=%dms", device_id_str, ping_ms);

            // 해당 디바이스 ping 업데이트
            for (uint8_t i = 0; i < s_tx.device_count; i++) {
                if (lora_device_id_equals(s_tx.devices[i].device_id, pong->device_id)) {
                    s_tx.devices[i].ping_ms = ping_ms;
                    break;
                }
            }
        }
    }

    return ESP_OK;
}

/**
 * @brief 상태 요청 태스크 (주기적)
 */
static void status_request_task(void* arg)
{
    T_LOGI(TAG, "상태 요청 태스크 시작 (주기: %d ms)", s_tx.request_interval_ms);

    while (s_mgr.running) {
        // 주기적 상태 요청
        send_status_request();

        // 대기
        vTaskDelay(pdMS_TO_TICKS(s_tx.request_interval_ms));
    }

    T_LOGI(TAG, "상태 요청 태스크 종료");
    vTaskDelete(nullptr);
}

#endif // DEVICE_MODE_TX

// ============================================================================
// RX 전용 (상태 요청 수신 시 응답 송신)
// ============================================================================

#ifdef DEVICE_MODE_RX

// RX 상태 캐시 (이벤트로 수집한 정보)
static struct {
    system_info_event_t system;      // EVT_INFO_UPDATED
    lora_rssi_event_t lora;          // EVT_LORA_RSSI_CHANGED
    bool system_valid;
    bool lora_valid;
} s_rx = {
    .system_valid = false,
    .lora_valid = false
};

/**
 * @brief 시스템 정보 이벤트 핸들러
 */
static esp_err_t on_info_updated(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const system_info_event_t* info = (const system_info_event_t*)event->data;
    s_rx.system = *info;
    s_rx.system_valid = true;

    return ESP_OK;
}

/**
 * @brief LoRa RSSI 이벤트 핸들러
 */
static esp_err_t on_lora_rssi_changed(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_rssi_event_t* rssi = (const lora_rssi_event_t*)event->data;
    s_rx.lora = *rssi;
    s_rx.lora_valid = true;

    return ESP_OK;
}

/**
 * @brief 상태 응답 송신
 */
static esp_err_t send_status_response(void)
{
    if (!s_rx.system_valid) {
        T_LOGW(TAG, "시스템 정보 없음, 응답 송신 생략");
        return ESP_ERR_INVALID_STATE;
    }

    lora_msg_status_t status;
    status.header = LORA_HDR_STATUS;

    // Device ID (system_info_event_t에서 가져오기)
    // TODO: device_id 문자열을 4바이트로 변환 필요
    status.device_id[0] = 0x01;  // 임시
    status.device_id[1] = 0x02;
    status.device_id[2] = 0x03;
    status.device_id[3] = 0x04;

    // 배터리, 전압, 온도
    status.battery = s_rx.system.battery;
    // voltage는 status 구조체에 없음

    // 카메라 ID (TODO: EVT_CAMERA_ID_CHANGED 이벤트로 수집)
    status.camera_id = 1;  // 임시

    // 업타임
    status.uptime = s_rx.system.uptime;

    // 밝기 (TODO: EVT_BRIGHTNESS_CHANGED 이벤트로 수집)
    status.brightness = 100;  // 임시

    // 플래그
    status.flags = s_rx.system.stopped ? LORA_STATUS_FLAG_STOPPED : 0;

    // 주파수/SyncWord
    status.frequency = s_rx.lora.frequency;
    status.sync_word = 0x12;  // TODO: lora_service에서 가져오기

    esp_err_t ret = lora_service_send((const uint8_t*)&status, sizeof(status));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(status.device_id, device_id_str);
        T_LOGI(TAG, "상태 응답 송신: ID=%s Bat=%d%% Up=%us", device_id_str, status.battery, status.uptime);
    } else {
        T_LOGE(TAG, "상태 응답 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief TX 명령 이벤트 핸들러 (0xE0~0xEF)
 */
static esp_err_t on_lora_tx_command(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;
    const uint8_t* data = packet->data;
    size_t len = packet->length;

    if (len == 0) {
        return ESP_OK;
    }

    uint8_t header = data[0];

    // 상태 요청 (0xE0)
    if (header == LORA_HDR_STATUS_REQ) {
        T_LOGI(TAG, "상태 요청 수신 (RSSI:%d)", packet->rssi);
        send_status_response();
    }
    // 기타 TX 명령은 추후 구현
    else {
        T_LOGD(TAG, "TX 명령 수신 (추후 구현): 0x%02X", header);
    }

    return ESP_OK;
}

#endif // DEVICE_MODE_RX

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

esp_err_t device_manager_init(void)
{
    if (s_mgr.initialized) {
        return ESP_OK;
    }

    T_LOGI(TAG, "Device Manager 초기화");

    s_mgr.initialized = true;
    s_mgr.running = false;

    return ESP_OK;
}

esp_err_t device_manager_start(void)
{
    if (!s_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mgr.running) {
        return ESP_OK;
    }

    s_mgr.running = true;

#ifdef DEVICE_MODE_TX
    // RX 응답 이벤트 구독 (0xD0~0xDF)
    event_bus_subscribe(EVT_LORA_RX_RESPONSE, on_lora_rx_response);

    // 상태 요청 태스크 시작
    BaseType_t ret = xTaskCreatePinnedToCore(
        status_request_task,
        "status_req",
        3072,
        nullptr,
        5,  // 우선순위
        nullptr,
        0   // Core 0
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "상태 요청 태스크 생성 실패");
        s_mgr.running = false;
        return ESP_FAIL;
    }
#endif

#ifdef DEVICE_MODE_RX
    // 시스템 정보 이벤트 구독
    event_bus_subscribe(EVT_INFO_UPDATED, on_info_updated);
    event_bus_subscribe(EVT_LORA_RSSI_CHANGED, on_lora_rssi_changed);

    // TX 명령 이벤트 구독 (0xE0~0xEF)
    event_bus_subscribe(EVT_LORA_TX_COMMAND, on_lora_tx_command);
#endif

    T_LOGI(TAG, "Device Manager 시작");
    return ESP_OK;
}

void device_manager_stop(void)
{
    if (!s_mgr.running) {
        return;
    }

    s_mgr.running = false;

#ifdef DEVICE_MODE_TX
    event_bus_unsubscribe(EVT_LORA_RX_RESPONSE, on_lora_rx_response);
#endif

#ifdef DEVICE_MODE_RX
    event_bus_unsubscribe(EVT_INFO_UPDATED, on_info_updated);
    event_bus_unsubscribe(EVT_LORA_RSSI_CHANGED, on_lora_rssi_changed);
    event_bus_unsubscribe(EVT_LORA_TX_COMMAND, on_lora_tx_command);
#endif

    T_LOGI(TAG, "Device Manager 정지");
}

void device_manager_deinit(void)
{
    device_manager_stop();
    s_mgr.initialized = false;
}

#ifdef DEVICE_MODE_TX

void device_manager_set_request_interval(uint32_t interval_ms)
{
    if (interval_ms < 1000) {
        interval_ms = 1000;  // 최소 1초
    }
    s_tx.request_interval_ms = interval_ms;
    T_LOGI(TAG, "상태 요청 주기 변경: %d ms", interval_ms);
}

esp_err_t device_manager_request_status_now(void)
{
    if (!s_mgr.running) {
        return ESP_ERR_INVALID_STATE;
    }
    return send_status_request();
}

#endif // DEVICE_MODE_TX

}  // extern "C"
