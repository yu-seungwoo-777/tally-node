/**
 * @file device_manager.cpp
 * @brief Device Manager 구현 (이벤트 기반)
 *
 * TX: 주기적 상태 요청, RX 디바이스 리스트 관리
 * RX: 상태 요청 수신 시 응답 송신
 *
 * 의존성: event_bus만 직접 사용
 *         나머지는 이벤트로 데이터 수신
 */

#include "device_manager.h"
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

#define STATUS_REQUEST_INTERVAL_MS  30000  // 기본 30초 (마지막 Tally 전송 후)
#define ONLINE_TIMEOUT_MS          90000  // 90초 (상태 요청 30초 × 3)
#define MAX_DEVICES  20

static struct {
    uint32_t request_interval_ms;
    uint32_t last_request_time;
    uint32_t last_tally_send_time;  // 마지막 Tally 전송 시간 (ms)
    device_info_t devices[MAX_DEVICES];
    uint8_t device_count;
    uint8_t device_limit;      // 라이센스 제한 (이벤트로 수신)
    bool limit_valid;          // 캐시 유효성 플래그
} s_tx = {
    .request_interval_ms = STATUS_REQUEST_INTERVAL_MS,
    .last_request_time = 0,
    .last_tally_send_time = 0,
    .devices = {},
    .device_count = 0,
    .device_limit = 0,         // 기본값: 무제한 (체크 안함)
    .limit_valid = false
};

/**
 * @brief 상태 요청 송신 (이벤트 기반)
 */
static esp_err_t send_status_request(void)
{
    // 정적 버퍼 (이벤트 버스가 비동기로 처리하므로)
    static uint8_t s_packet[] = { LORA_HDR_STATUS_REQ };

    lora_send_request_t req = {
        .data = s_packet,
        .length = sizeof(s_packet)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        s_tx.last_request_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        s_tx.last_tally_send_time = s_tx.last_request_time;  // 상태 요청도 갱신
        T_LOGI(TAG, "상태 요청 송신 (Broadcast)");
    } else {
        T_LOGE(TAG, "상태 요청 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief 기능정지 명령 송신 (0xE4) (이벤트 기반)
 */
static esp_err_t send_stop_command(const uint8_t* device_id)
{
    // 정적 버퍼 (이벤트 버스가 비동기로 처리하므로)
    static lora_cmd_stop_t s_stop;

    s_stop.header = LORA_HDR_STOP;
    memcpy(s_stop.device_id, device_id, LORA_DEVICE_ID_LEN);

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_stop,
        .length = sizeof(s_stop)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGW(TAG, "기능정지 명령 송신: ID=%s", device_id_str);
    } else {
        T_LOGE(TAG, "기능정지 명령 송신 실패: %d", ret);
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

    T_LOGI(TAG, "상태 수신: ID=%s Bat=%d%% Cam=%d Up=%us Freq=%u SW=0x%02X (RSSI:%d SNR:%.1f)",
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
    bool is_new_device = (found_idx < 0);
    if (is_new_device) {
        // 새 디바이스 추가
        if (s_tx.device_count < MAX_DEVICES) {
            found_idx = s_tx.device_count++;
            memcpy(s_tx.devices[found_idx].device_id, status->device_id, LORA_DEVICE_ID_LEN);
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
    s_tx.devices[found_idx].last_seen = now;
    s_tx.devices[found_idx].frequency = (float)status->frequency;
    s_tx.devices[found_idx].sync_word = status->sync_word;
    s_tx.devices[found_idx].is_online = true;  // 상태 응답 수신 = 온라인

    // 라이센스 device_limit 체크 (새 디바이스 추가 시만)
    if (is_new_device) {
        uint8_t device_limit = s_tx.device_limit;  // 캐시된 값 사용

        // device_limit == 0: 라이센스 미등록 상태 (유예 기간 포함)
        // device_limit > 0: 등록된 상태
        if (device_limit == 0) {
            // 라이센스 미등록: device_limit 체크 건너뜀
            T_LOGD(TAG, "라이센스 미등록 상태 (device_limit=0)");
        } else if (s_tx.device_count > device_limit) {
            // 등록된 디바이스 수가 제한 초과: LIFO (가장 최근 디바이스 제거)
            uint8_t remove_idx = s_tx.device_count - 1;  // 마지막 디바이스
            const uint8_t* remove_device_id = s_tx.devices[remove_idx].device_id;

            char remove_id_str[5];
            lora_device_id_to_str(remove_device_id, remove_id_str);
            T_LOGW(TAG, "device_limit 초과 (%d/%d), 디바이스 제거: ID=%s",
                     s_tx.device_count, device_limit, remove_id_str);

            // 기능정지 명령 송신
            send_stop_command(remove_device_id);

            // 리스트에서 제거
            s_tx.device_count--;
        }
    }

    // 디바이스 리스트 변경 이벤트 발행
    device_list_event_t list_event;
    memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * s_tx.device_count);
    list_event.count = s_tx.device_count;
    list_event.registered_count = s_tx.device_count;
    event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
}

/**
 * @brief 라이센스 상태 변경 이벤트 핸들러
 */
static esp_err_t on_license_state_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const license_state_event_t* license = (const license_state_event_t*)event->data;

    s_tx.device_limit = license->device_limit;
    s_tx.limit_valid = true;

    T_LOGI(TAG, "라이센스 상태 변경: limit=%d, state=%d, grace=%u",
             license->device_limit, license->state, license->grace_remaining);

    return ESP_OK;
}

/**
 * @brief RX 응답 이벤트 핸들러 (0xD0~0xDF)
 */
static esp_err_t on_lora_rx_response(const event_data_t* event)
{
    if (!event) {
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
 * @brief LoRa 패킷 전송 이벤트 핸들러
 * Tally 전송 시간을 추적해서 상태 요청 타이밍 조정
 */
static esp_err_t on_lora_packet_sent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;

    // Tally 패킷(0xF1~0xF4) 전송 시 기록
    if (packet->length > 0) {
        uint8_t header = packet->data[0];
        if (LORA_IS_TALLY_HEADER(header)) {
            s_tx.last_tally_send_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            T_LOGD(TAG, "Tally 전송 기록: header=0x%02X", header);
        }
    }

    return ESP_OK;
}

/**
 * @brief 오프라인 디바이스 체크 및 상태 업데이트
 */
static void check_offline_devices(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool list_changed = false;

    for (uint8_t i = 0; i < s_tx.device_count; i++) {
        // tick 카운터 차이 계산 (오버플로우 고려)
        uint32_t elapsed = now - s_tx.devices[i].last_seen;

        bool was_online = s_tx.devices[i].is_online;
        bool should_be_online = (elapsed < ONLINE_TIMEOUT_MS);

        s_tx.devices[i].is_online = should_be_online;

        // 온라인 → 오프라인 변경 시
        if (was_online && !should_be_online) {
            char device_id_str[5];
            lora_device_id_to_str(s_tx.devices[i].device_id, device_id_str);
            T_LOGW(TAG, "디바이스 오프라인: ID=%s (무응답 %u초)", device_id_str, elapsed / 1000);
            list_changed = true;
        }
    }

    // 상태 변경 시 이벤트 발행
    if (list_changed) {
        device_list_event_t list_event;
        memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * s_tx.device_count);
        list_event.count = s_tx.device_count;
        list_event.registered_count = s_tx.device_count;
        event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
    }
}

/**
 * @brief 상태 요청 태스크 (마지막 Tally 전송 후 30초 경과 시)
 */
static void status_request_task(void* arg)
{
    T_LOGI(TAG, "상태 요청 태스크 시작 (마지막 Tally 전송 후 %d ms)", s_tx.request_interval_ms);

    while (s_mgr.running) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 오프라인 디바이스 체크
        check_offline_devices();

        // 마지막 Tally 전송 후 request_interval_ms 경과 시 상태 요청
        uint32_t elapsed = now - s_tx.last_tally_send_time;

        // 디버깅: 10초마다 경과 시간 로그
        static uint32_t last_log_time = 0;
        if (now - last_log_time >= 10000) {
            T_LOGD(TAG, "Tally 경과: %u ms (요청 threshold: %u ms)", elapsed, s_tx.request_interval_ms);
            last_log_time = now;
        }

        if (elapsed >= s_tx.request_interval_ms) {
            send_status_request();
        }

        // 1초마다 체크
        vTaskDelay(pdMS_TO_TICKS(1000));
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
    .system = {},
    .lora = {},
    .system_valid = false,
    .lora_valid = false
};

/**
 * @brief 시스템 정보 이벤트 핸들러
 */
static esp_err_t on_info_updated(const event_data_t* event)
{
    if (!event) {
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
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_rssi_event_t* rssi = (const lora_rssi_event_t*)event->data;
    s_rx.lora = *rssi;
    s_rx.lora_valid = true;

    return ESP_OK;
}

/**
 * @brief 상태 응답 송신 (이벤트 기반)
 */
static esp_err_t send_status_response(void)
{
    if (!s_rx.system_valid) {
        T_LOGW(TAG, "시스템 정보 없음, 응답 송신 생략");
        return ESP_ERR_INVALID_STATE;
    }

    // 정적 버퍼 (이벤트 버스가 비동기로 처리하므로)
    static lora_msg_status_t s_status;
    s_status.header = LORA_HDR_STATUS;

    // Device ID (system_info_event_t에서 변환)
    // device_id는 "2D20" 같은 4자리 hex 문자열 → 2바이트로 변환
    if (strlen(s_rx.system.device_id) == 4) {
        char hex_str[3];
        hex_str[0] = s_rx.system.device_id[0];
        hex_str[1] = s_rx.system.device_id[1];
        hex_str[2] = '\0';
        s_status.device_id[0] = (uint8_t)strtol(hex_str, NULL, 16);

        hex_str[0] = s_rx.system.device_id[2];
        hex_str[1] = s_rx.system.device_id[3];
        hex_str[2] = '\0';
        s_status.device_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
    } else {
        s_status.device_id[0] = 0x2D;
        s_status.device_id[1] = 0x20;
    }

    // 배터리, 전압, 온도
    s_status.battery = s_rx.system.battery;
    // voltage는 status 구조체에 없음

    // 카메라 ID (TODO: EVT_CAMERA_ID_CHANGED 이벤트로 수집)
    s_status.camera_id = 1;  // 임시

    // 업타임
    s_status.uptime = s_rx.system.uptime;

    // 밝기 (TODO: EVT_BRIGHTNESS_CHANGED 이벤트로 수집)
    s_status.brightness = 100;  // 임시

    // 주파수/SyncWord
    s_status.frequency = (uint16_t)s_rx.lora.frequency;
    s_status.sync_word = 0x12;  // TODO: lora_service에서 가져오기

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_status,
        .length = sizeof(s_status)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(s_status.device_id, device_id_str);
        T_LOGI(TAG, "상태 응답 송신: ID=%s Bat=%d%% Up=%us", device_id_str, s_status.battery, s_status.uptime);
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
    if (!event) {
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

    // LoRa 패킷 전송 이벤트 구독 (Tally 전송 시간 추적)
    event_bus_subscribe(EVT_LORA_PACKET_SENT, on_lora_packet_sent);

    // 라이센스 상태 변경 이벤트 구독
    event_bus_subscribe(EVT_LICENSE_STATE_CHANGED, on_license_state_changed);

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
    event_bus_unsubscribe(EVT_LORA_PACKET_SENT, on_lora_packet_sent);
    event_bus_unsubscribe(EVT_LICENSE_STATE_CHANGED, on_license_state_changed);
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
