/**
 * @file device_manager.cpp
 * @brief Device Manager 구현 (이벤트 기반)
 *
 * TX: 주기적 상태 요청, RX 디바이스 리스트 관리
 * RX: 상태 요청 수신 시 응답 송신, 밝기/카메라ID 설정 명령 처리
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
#include "esp_random.h"
#include "esp_system.h"
#include <string.h>

static const char* TAG = "DeviceMgr";

// ============================================================================
// 전방 선언 (static 함수)
// ============================================================================
#ifdef DEVICE_MODE_TX
static esp_err_t send_status_request(void);
#endif
#ifdef DEVICE_MODE_RX
static esp_err_t send_status_response(void);
#endif

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

// ============================================================================
// 테스트 모드 상태 (TX 전용)
// ============================================================================

static bool s_test_mode_running = false;

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
    // 테스트 모드 실행 중이면 기능정지 송신 스킵
    if (s_test_mode_running) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGW(TAG, "테스트 모드 실행 중: 기능정지 송신 스킵 (ID=%s)", device_id_str);
        return ESP_OK;
    }

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
 * @brief 밝기 설정 명령 송신 (0xE1) (이벤트 기반)
 */
static esp_err_t send_brightness_command(const uint8_t* device_id, uint8_t brightness)
{
    static lora_cmd_brightness_t s_cmd;

    s_cmd.header = LORA_HDR_SET_BRIGHTNESS;
    memcpy(s_cmd.device_id, device_id, LORA_DEVICE_ID_LEN);
    s_cmd.brightness = brightness;

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_cmd,
        .length = sizeof(s_cmd)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGI(TAG, "밝기 설정 명령 송신: ID=%s 밝기=%d", device_id_str, brightness);
    } else {
        T_LOGE(TAG, "밝기 설정 명령 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief 카메라 ID 설정 명령 송신 (0xE2) (이벤트 기반)
 */
static esp_err_t send_camera_id_command(const uint8_t* device_id, uint8_t camera_id)
{
    static lora_cmd_camera_id_t s_cmd;

    s_cmd.header = LORA_HDR_SET_CAMERA_ID;
    memcpy(s_cmd.device_id, device_id, LORA_DEVICE_ID_LEN);
    s_cmd.camera_id = camera_id;

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_cmd,
        .length = sizeof(s_cmd)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGI(TAG, "카메라 ID 설정 명령 송신: ID=%s CameraID=%d", device_id_str, camera_id);
    } else {
        T_LOGE(TAG, "카메라 ID 설정 명령 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief 재부팅 명령 송신 (0xE5) (이벤트 기반)
 */
static esp_err_t send_reboot_command(const uint8_t* device_id)
{
    static lora_cmd_reboot_t s_cmd;

    s_cmd.header = LORA_HDR_REBOOT;
    memcpy(s_cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_cmd,
        .length = sizeof(s_cmd)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGW(TAG, "재부팅 명령 송신: ID=%s", device_id_str);
    } else {
        T_LOGE(TAG, "재부팅 명령 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief PING 명령 송신 (0xE6) (이벤트 기반)
 */
static esp_err_t send_ping_command(const uint8_t* device_id)
{
    // 정적 버퍼
    static lora_cmd_ping_t s_ping;

    s_ping.header = LORA_HDR_PING;
    memcpy(s_ping.device_id, device_id, LORA_DEVICE_ID_LEN);
    // 송신 시간 하위 2바이트 (ms)
    s_ping.timestamp_low = (uint16_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    // 구조체 크기 확인 (디버그)
    T_LOGI(TAG, "PING struct size: %zu (header=%d, id[2], ts=%u)",
             sizeof(s_ping), s_ping.header, s_ping.timestamp_low);

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_ping,
        .length = sizeof(s_ping)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGI(TAG, "PING 송신: ID=%s, TS=%u, len=%zu",
                 device_id_str, s_ping.timestamp_low, req.length);
    } else {
        T_LOGE(TAG, "PING 송신 실패");
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
        // 라이선스 제한 먼저 체크 (디바이스 추가 전)
        uint8_t device_limit = s_tx.device_limit;

        // device_limit == 0: 라이센스 미등록 상태로 모든 디바이스 거부
        // device_limit > 0: 등록된 limit까지만 허용
        if (s_tx.device_count >= device_limit) {
            // 라이센스 초과: 디바이스 추가하지 않고 기능정지 송신
            char id_str[5];
            lora_device_id_to_str(status->device_id, id_str);
            T_LOGW(TAG, "라이센스 device_limit 초과 (%d/%d), 기능정지 송신: ID=%s",
                     s_tx.device_count, device_limit, id_str);

            send_stop_command(status->device_id);
            return;
        }

        // MAX_DEVICES 체크
        if (s_tx.device_count >= MAX_DEVICES) {
            T_LOGW(TAG, "디바이스 리스트 꽉참 (%d)", MAX_DEVICES);
            return;
        }

        // 안전한 경우만 디바이스 추가
        found_idx = s_tx.device_count++;
        memcpy(s_tx.devices[found_idx].device_id, status->device_id, LORA_DEVICE_ID_LEN);
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
    s_tx.devices[found_idx].is_stopped = (status->stopped == 1);
    s_tx.devices[found_idx].is_online = true;  // 상태 응답 수신 = 온라인

    // 디바이스-카메라 매핑 이벤트 발행 (ConfigService에서 NVS 저장)
    uint8_t cam_map_data[3] = {status->device_id[0], status->device_id[1], status->camera_id};
    event_bus_publish(EVT_DEVICE_CAM_MAP_RECEIVE, cam_map_data, sizeof(cam_map_data));

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

// ============================================================================
// 디바이스 제어 이벤트 핸들러 (TX 전용)
// ============================================================================

/**
 * @brief 디바이스 밝기 설정 요청 이벤트 핸들러
 */
static esp_err_t on_device_brightness_request(const event_data_t* event)
{
    if (!event || event->data_size < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = event->data;
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t brightness = data[2];

    return send_brightness_command(device_id, brightness);
}

/**
 * @brief 디바이스 카메라 ID 설정 요청 이벤트 핸들러
 */
static esp_err_t on_device_camera_id_request(const event_data_t* event)
{
    if (!event || event->data_size < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = event->data;
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t camera_id = data[2];

    return send_camera_id_command(device_id, camera_id);
}

/**
 * @brief 디바이스 PING 요청 이벤트 핸들러
 */
static esp_err_t on_device_ping_request(const event_data_t* event)
{
    if (!event || event->data_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* device_id = event->data;
    return send_ping_command(device_id);
}

/**
 * @brief 디바이스 기능 정지 요청 이벤트 핸들러
 */
static esp_err_t on_device_stop_request(const event_data_t* event)
{
    if (!event || event->data_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* device_id = event->data;
    return send_stop_command(device_id);
}

/**
 * @brief 디바이스 재부팅 요청 이벤트 핸들러
 */
static esp_err_t on_device_reboot_request(const event_data_t* event)
{
    if (!event || event->data_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* device_id = event->data;
    return send_reboot_command(device_id);
}

/**
 * @brief 상태 요청 이벤트 핸들러
 */
static esp_err_t on_status_request(const event_data_t* event)
{
    (void)event;
    return send_status_request();
}

/**
 * @brief 테스트 모드 시작 이벤트 핸들러
 */
static esp_err_t on_test_mode_start(const event_data_t* event)
{
    (void)event;
    s_test_mode_running = true;
    T_LOGI(TAG, "테스트 모드 시작: 기능정지 송신 비활성화");
    return ESP_OK;
}

/**
 * @brief 테스트 모드 중지 이벤트 핸들러
 */
static esp_err_t on_test_mode_stop(const event_data_t* event)
{
    (void)event;
    s_test_mode_running = false;
    T_LOGI(TAG, "테스트 모드 중지: 기능정지 송신 활성화");
    return ESP_OK;
}

/**
 * @brief 디바이스 카메라 매핑 수신 이벤트 핸들러
 * @note NVS에서 로드된 매핑 또는 상태 응답에서 수신
 */
static esp_err_t on_device_cam_map_receive(const event_data_t* event)
{
    if (event->type != EVT_DEVICE_CAM_MAP_RECEIVE) {
        return ESP_OK;
    }

    if (event->data_size < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* data = (const uint8_t*)event->data;
    uint8_t device_id[2] = {data[0], data[1]};
    uint8_t camera_id = data[2];

    // 디바이스 리스트에서 찾기
    for (uint8_t i = 0; i < s_tx.device_count; i++) {
        if (s_tx.devices[i].device_id[0] == device_id[0] &&
            s_tx.devices[i].device_id[1] == device_id[1]) {
            // 기존 디바이스 카메라 ID 업데이트
            s_tx.devices[i].camera_id = camera_id;
            T_LOGI(TAG, "디바이스 카메라 ID 로드: [%02X%02X] → Cam%d",
                    device_id[0], device_id[1], camera_id);
            return ESP_OK;
        }
    }

    // 디바이스가 리스트에 없으면 추가 (오프라인 상태로)
    if (s_tx.device_count < MAX_DEVICES) {
        uint8_t idx = s_tx.device_count++;
        s_tx.devices[idx].device_id[0] = device_id[0];
        s_tx.devices[idx].device_id[1] = device_id[1];
        s_tx.devices[idx].camera_id = camera_id;
        s_tx.devices[idx].is_online = false;  // 아직 온라인 아님
        s_tx.devices[idx].last_seen = 0;
        T_LOGI(TAG, "디바이스 카메라 ID 로드 (오프라인): [%02X%02X] → Cam%d",
                device_id[0], device_id[1], camera_id);

        // 웹 서버에 디바이스 리스트 변경 알림
        device_list_event_t list_event;
        memset(&list_event, 0, sizeof(list_event));
        memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * s_tx.device_count);
        list_event.count = s_tx.device_count;
        list_event.registered_count = s_tx.device_count;
        event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
    }

    return ESP_OK;
}

/**
 * @brief 디바이스 등록 해제 이벤트 핸들러
 * @note 디바이스 리스트에서 제거
 */
static esp_err_t on_device_unregister(const event_data_t* event)
{
    if (event->type != EVT_DEVICE_UNREGISTER) {
        return ESP_OK;
    }

    const device_register_event_t* req = (const device_register_event_t*)event->data;
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    // 디바이스 리스트에서 찾기
    for (uint8_t i = 0; i < s_tx.device_count; i++) {
        if (s_tx.devices[i].device_id[0] == req->device_id[0] &&
            s_tx.devices[i].device_id[1] == req->device_id[1]) {

            // 찾은 디바이스를 삭제 (마지막 디바이스를 가져옴)
            s_tx.device_count--;
            if (i < s_tx.device_count) {
                memcpy(&s_tx.devices[i], &s_tx.devices[s_tx.device_count], sizeof(device_info_t));
            }

            T_LOGI(TAG, "디바이스 리스트에서 제거: [%02X%02X]",
                    req->device_id[0], req->device_id[1]);

            // 웹 서버에 디바이스 리스트 변경 알림
            device_list_event_t list_event;
            memset(&list_event, 0, sizeof(list_event));
            memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * s_tx.device_count);
            list_event.count = s_tx.device_count;
            list_event.registered_count = s_tx.device_count;
            event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));

            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
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
    uint8_t brightness;              // 현재 밝기 (0-255)
    uint8_t camera_id;               // 현재 카메라 ID (1-20)
    bool stopped;                    // 기능 정지 상태
    bool system_valid;
    bool lora_valid;
} s_rx = {
    .system = {},
    .lora = {},
    .brightness = 255,  // 기본값
    .camera_id = 1,      // 기본값
    .stopped = false,    // 기본값: 정상
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
 * @brief 밝기 변경 이벤트 핸들러
 */
static esp_err_t on_brightness_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* brightness = (const uint8_t*)event->data;
    s_rx.brightness = *brightness;
    T_LOGI(TAG, "밝기 변경: %d", s_rx.brightness);

    // 변경 즉시 상태 응답 송신 (TX에 새 값 알림)
    send_status_response();

    return ESP_OK;
}

/**
 * @brief 카메라 ID 변경 이벤트 핸들러
 */
static esp_err_t on_camera_id_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t* camera_id = (const uint8_t*)event->data;
    s_rx.camera_id = *camera_id;
    T_LOGI(TAG, "카메라 ID 변경: %d", s_rx.camera_id);

    // 변경 즉시 상태 응답 송신 (TX에 새 값 알림)
    send_status_response();

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

    // 카메라 ID (EVT_CAMERA_ID_CHANGED 이벤트로 수집)
    s_status.camera_id = s_rx.camera_id;

    // 업타임
    s_status.uptime = s_rx.system.uptime;

    // 밝기 (EVT_BRIGHTNESS_CHANGED 이벤트로 수집)
    s_status.brightness = s_rx.brightness;

    // 주파수/SyncWord
    s_status.frequency = (uint16_t)s_rx.lora.frequency;
    s_status.sync_word = 0x12;  // TODO: lora_service에서 가져오기

    // 기능 정지 상태
    s_status.stopped = s_rx.stopped ? 1 : 0;

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_status,
        .length = sizeof(s_status)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(s_status.device_id, device_id_str);
        T_LOGI(TAG, "상태 응답 송신: ID=%s Bat=%d%% Up=%us Stop=%d",
                 device_id_str, s_status.battery, s_status.uptime, s_status.stopped);
    } else {
        T_LOGE(TAG, "상태 응답 송신 실패: %d", ret);
    }

    return ret;
}

/**
 * @brief PONG 응답 송신 (0xD2)
 */
static esp_err_t send_pong_response(const uint8_t* device_id, uint16_t tx_timestamp_low)
{
    if (!s_rx.system_valid) {
        T_LOGW(TAG, "시스템 정보 없음, PONG 송신 생략");
        return ESP_ERR_INVALID_STATE;
    }

    // 정적 버퍼
    static lora_msg_pong_t s_pong;
    s_pong.header = LORA_HDR_PONG;
    memcpy(s_pong.device_id, device_id, LORA_DEVICE_ID_LEN);
    s_pong.tx_timestamp_low = tx_timestamp_low;

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_pong,
        .length = sizeof(s_pong)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char device_id_str[5];
        lora_device_id_to_str(device_id, device_id_str);
        T_LOGI(TAG, "PONG 송신: ID=%s, TS=%u", device_id_str, tx_timestamp_low);
    } else {
        T_LOGE(TAG, "PONG 송신 실패: %d", ret);
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

        // 충돌 방지를 위한 랜덤 지연 (0-1000ms)
        uint32_t delay_ms = esp_random() % 1000;
        T_LOGD(TAG, "상태 응답 지연: %u ms", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        send_status_response();
    }
    // 밝기 설정 (0xE1)
    else if (header == LORA_HDR_SET_BRIGHTNESS) {
        if (len >= sizeof(lora_cmd_brightness_t)) {
            const lora_cmd_brightness_t* cmd = (const lora_cmd_brightness_t*)data;

            // Broadcast 또는 자신의 Device ID 확인
            bool is_target = lora_device_id_is_broadcast(cmd->device_id);

            if (!is_target && strlen(s_rx.system.device_id) == 4) {
                // 자신의 ID와 비교
                uint8_t my_id[2];
                char hex_str[3];
                hex_str[0] = s_rx.system.device_id[0];
                hex_str[1] = s_rx.system.device_id[1];
                hex_str[2] = '\0';
                my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);
                hex_str[0] = s_rx.system.device_id[2];
                hex_str[1] = s_rx.system.device_id[3];
                my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
                is_target = lora_device_id_equals(my_id, cmd->device_id);
            }

            if (is_target) {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGI(TAG, "밝기 설정 수신: ID=%s, 밝기=%d", device_id_str, cmd->brightness);

                // 밝기 변경 이벤트 발행 (led_service가 구독)
                event_bus_publish(EVT_BRIGHTNESS_CHANGED, &cmd->brightness, sizeof(cmd->brightness));
            } else {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGD(TAG, "밝기 명령 무시: 타겟 ID=%s (내 ID가 아님)", device_id_str);
            }
        } else {
            T_LOGW(TAG, "밝기 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_brightness_t));
        }
    }
    // 카메라 ID 설정 (0xE2)
    else if (header == LORA_HDR_SET_CAMERA_ID) {
        if (len >= sizeof(lora_cmd_camera_id_t)) {
            const lora_cmd_camera_id_t* cmd = (const lora_cmd_camera_id_t*)data;

            // Broadcast 또는 자신의 Device ID 확인
            bool is_target = lora_device_id_is_broadcast(cmd->device_id);

            if (!is_target && strlen(s_rx.system.device_id) == 4) {
                // 자신의 ID와 비교
                uint8_t my_id[2];
                char hex_str[3];
                hex_str[0] = s_rx.system.device_id[0];
                hex_str[1] = s_rx.system.device_id[1];
                hex_str[2] = '\0';
                my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);
                hex_str[0] = s_rx.system.device_id[2];
                hex_str[1] = s_rx.system.device_id[3];
                my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
                is_target = lora_device_id_equals(my_id, cmd->device_id);
            }

            if (is_target) {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGI(TAG, "카메라 ID 설정 수신: ID=%s, CameraID=%d", device_id_str, cmd->camera_id);

                // 이벤트 발행 (config_service가 NVS 저장)
                event_bus_publish(EVT_CAMERA_ID_CHANGED, &cmd->camera_id, sizeof(cmd->camera_id));
            } else {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGD(TAG, "카메라 ID 명령 무시: 타겟 ID=%s (내 ID가 아님)", device_id_str);
            }
        } else {
            T_LOGW(TAG, "카메라 ID 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_camera_id_t));
        }
    }
    // RF 설정 (0xE3) - Broadcast format: [0xE3][frequency(4)][sync_word(1)]
    else if (header == LORA_HDR_SET_RF) {
        if (len == 6) {
            float frequency;
            memcpy(&frequency, &data[1], sizeof(float));
            uint8_t sync_word = data[5];

            T_LOGI(TAG, "RF 설정 수신: %.1f MHz, Sync 0x%02X", frequency, sync_word);

            // 이벤트 발행 (lora_service가 드라이버 적용)
            lora_rf_event_t rf_event = {
                .frequency = frequency,
                .sync_word = sync_word
            };
            event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
        } else {
            T_LOGW(TAG, "RF 명령 길이 오류: %d (예상: 6)", len);
        }
    }
    // 전역 밝기 Broadcast (0xE7) - device_id 없음, 모든 RX가 처리
    else if (header == LORA_HDR_BRIGHTNESS_BROADCAST) {
        if (len >= sizeof(lora_cmd_brightness_broadcast_t)) {
            const lora_cmd_brightness_broadcast_t* cmd = (const lora_cmd_brightness_broadcast_t*)data;
            T_LOGI(TAG, "전역 밝기 설정 수신 (Broadcast): %d", cmd->brightness);

            // 밝기 변경 이벤트 발행 (led_service가 구독)
            event_bus_publish(EVT_BRIGHTNESS_CHANGED, &cmd->brightness, sizeof(cmd->brightness));
        } else {
            T_LOGW(TAG, "전역 밝기 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_brightness_broadcast_t));
        }
    }
    // 기능 정지 (0xE4)
    else if (header == LORA_HDR_STOP) {
        if (len >= sizeof(lora_cmd_stop_t)) {
            const lora_cmd_stop_t* cmd = (const lora_cmd_stop_t*)data;

            // Broadcast 또는 자신의 Device ID 확인
            bool is_target = lora_device_id_is_broadcast(cmd->device_id);

            if (!is_target && strlen(s_rx.system.device_id) == 4) {
                // 자신의 ID와 비교
                uint8_t my_id[2];
                char hex_str[3];
                hex_str[0] = s_rx.system.device_id[0];
                hex_str[1] = s_rx.system.device_id[1];
                hex_str[2] = '\0';
                my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);
                hex_str[0] = s_rx.system.device_id[2];
                hex_str[1] = s_rx.system.device_id[3];
                my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
                is_target = lora_device_id_equals(my_id, cmd->device_id);
            }

            if (is_target) {
                s_rx.stopped = true;
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGW(TAG, "기능 정지 명령 수신: ID=%s, 디스플레이/LED 정지", device_id_str);

                // 기능 정지 이벤트 발행 (다른 서비스에서 처리)
                bool stopped_val = true;
                event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));
            }
        } else {
            T_LOGW(TAG, "기능 정지 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_stop_t));
        }
    }
    // 재부팅 (0xE5)
    else if (header == LORA_HDR_REBOOT) {
        if (len >= sizeof(lora_cmd_reboot_t)) {
            const lora_cmd_reboot_t* cmd = (const lora_cmd_reboot_t*)data;

            // Broadcast ID (0xFF 0xFF) 확인
            bool is_broadcast = (cmd->device_id[0] == 0xFF && cmd->device_id[1] == 0xFF);

            // 자신의 Device ID 확인
            bool is_target = is_broadcast;  // Broadcast는 모든 디바이스 대상

            if (!is_broadcast && strlen(s_rx.system.device_id) == 4) {
                // 자신의 ID와 비교
                uint8_t my_id[2];
                char hex_str[3];
                hex_str[0] = s_rx.system.device_id[0];
                hex_str[1] = s_rx.system.device_id[1];
                hex_str[2] = '\0';
                my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);
                hex_str[0] = s_rx.system.device_id[2];
                hex_str[1] = s_rx.system.device_id[3];
                my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
                is_target = lora_device_id_equals(my_id, cmd->device_id);
            }

            if (is_target) {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                if (is_broadcast) {
                    T_LOGW(TAG, "브로드캐스트 재부팅 명령 수신, 1초 후 재부팅...");
                } else {
                    T_LOGW(TAG, "재부팅 명령 수신: ID=%s, 1초 후 재부팅...", device_id_str);
                }

                // 응답 ACK 전송
                // TODO: ACK 전송 (나중에 구현)

                // 1초 대기 후 재부팅
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
        } else {
            T_LOGW(TAG, "재부팅 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_reboot_t));
        }
    }
    // PING (0xE6)
    else if (header == LORA_HDR_PING) {
        // 디버그: 수신된 데이터 출력
        T_LOGI(TAG, "PING 수신: len=%d, expected=%zu", len, sizeof(lora_cmd_ping_t));
        for (int i = 0; i < len && i < 8; i++) {
            T_LOGI(TAG, "  data[%d]: 0x%02X", i, data[i]);
        }

        if (len >= sizeof(lora_cmd_ping_t)) {
            const lora_cmd_ping_t* cmd = (const lora_cmd_ping_t*)data;

            // Broadcast 또는 자신의 Device ID 확인
            bool is_target = lora_device_id_is_broadcast(cmd->device_id);

            if (!is_target && strlen(s_rx.system.device_id) == 4) {
                // 자신의 ID와 비교
                uint8_t my_id[2];
                char hex_str[3];
                hex_str[0] = s_rx.system.device_id[0];
                hex_str[1] = s_rx.system.device_id[1];
                hex_str[2] = '\0';
                my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);
                hex_str[0] = s_rx.system.device_id[2];
                hex_str[1] = s_rx.system.device_id[3];
                my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);
                is_target = lora_device_id_equals(my_id, cmd->device_id);
            }

            if (is_target) {
                char device_id_str[5];
                lora_device_id_to_str(cmd->device_id, device_id_str);
                T_LOGI(TAG, "PING 수신: ID=%s, TS=%u", device_id_str, cmd->timestamp_low);

                // PONG 응답 송신
                send_pong_response(cmd->device_id, cmd->timestamp_low);
            }
        } else {
            T_LOGW(TAG, "PING 명령 길이 부족: %d < %zu", len, sizeof(lora_cmd_ping_t));
        }
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

    // 디바이스 제어 이벤트 구독
    event_bus_subscribe(EVT_DEVICE_BRIGHTNESS_REQUEST, on_device_brightness_request);
    event_bus_subscribe(EVT_DEVICE_CAMERA_ID_REQUEST, on_device_camera_id_request);
    event_bus_subscribe(EVT_DEVICE_PING_REQUEST, on_device_ping_request);
    event_bus_subscribe(EVT_DEVICE_STOP_REQUEST, on_device_stop_request);
    event_bus_subscribe(EVT_DEVICE_REBOOT_REQUEST, on_device_reboot_request);
    event_bus_subscribe(EVT_STATUS_REQUEST, on_status_request);
    event_bus_subscribe(EVT_DEVICE_UNREGISTER, on_device_unregister);

    // 테스트 모드 이벤트 구독
    event_bus_subscribe(EVT_TALLY_TEST_MODE_START, on_test_mode_start);
    event_bus_subscribe(EVT_TALLY_TEST_MODE_STOP, on_test_mode_stop);

    // 디바이스 카메라 매핑 이벤트 구독 (NVS 로드된 매핑 수신)
    event_bus_subscribe(EVT_DEVICE_CAM_MAP_RECEIVE, on_device_cam_map_receive);

    // NVS에 저장된 디바이스-카메라 매핑 로드 요청
    event_bus_publish(EVT_DEVICE_CAM_MAP_LOAD, nullptr, 0);

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
    event_bus_subscribe(EVT_BRIGHTNESS_CHANGED, on_brightness_changed);
    event_bus_subscribe(EVT_CAMERA_ID_CHANGED, on_camera_id_changed);

    // TX 명령 이벤트 구독 (0xE0~0xEF)
    event_bus_subscribe(EVT_LORA_TX_COMMAND, on_lora_tx_command);

    // 부팅 직후 상태 응답 송신 (시스템 정보 준비 대기 후)
    // 충돌 방지를 위해 랜덤 지연 (0-2000ms) 후 송신
    uint32_t boot_delay_ms = esp_random() % 2000;
    T_LOGI(TAG, "부팅 후 %u ms 지연 후 상태 응답 송신 예정", boot_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(boot_delay_ms));
    send_status_response();
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
    event_bus_unsubscribe(EVT_DEVICE_BRIGHTNESS_REQUEST, on_device_brightness_request);
    event_bus_unsubscribe(EVT_DEVICE_CAMERA_ID_REQUEST, on_device_camera_id_request);
    event_bus_unsubscribe(EVT_DEVICE_PING_REQUEST, on_device_ping_request);
    event_bus_unsubscribe(EVT_DEVICE_STOP_REQUEST, on_device_stop_request);
    event_bus_unsubscribe(EVT_DEVICE_REBOOT_REQUEST, on_device_reboot_request);
    event_bus_unsubscribe(EVT_STATUS_REQUEST, on_status_request);

    // 테스트 모드 이벤트 구독 해제
    event_bus_unsubscribe(EVT_TALLY_TEST_MODE_START, on_test_mode_start);
    event_bus_unsubscribe(EVT_TALLY_TEST_MODE_STOP, on_test_mode_stop);
#endif

#ifdef DEVICE_MODE_RX
    event_bus_unsubscribe(EVT_INFO_UPDATED, on_info_updated);
    event_bus_unsubscribe(EVT_LORA_RSSI_CHANGED, on_lora_rssi_changed);
    event_bus_unsubscribe(EVT_BRIGHTNESS_CHANGED, on_brightness_changed);
    event_bus_unsubscribe(EVT_CAMERA_ID_CHANGED, on_camera_id_changed);
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

esp_err_t device_manager_send_ping(const uint8_t* device_id)
{
    if (!s_mgr.running) {
        return ESP_ERR_INVALID_STATE;
    }

    // NULL이면 Broadcast ID 사용
    uint8_t target_id[2];
    if (device_id == NULL) {
        target_id[0] = 0xFF;
        target_id[1] = 0xFF;
        device_id = target_id;
    }

    return send_ping_command(device_id);
}

#endif // DEVICE_MODE_TX

}  // extern "C"
