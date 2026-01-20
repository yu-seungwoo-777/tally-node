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
#include "error_macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_system.h"
#include <string.h>

static const char* TAG = "03_Device";

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
        T_LOGI(TAG, "status request sent (Broadcast)");
    } else {
        T_LOGE(TAG, "status request send failed: %d", ret);
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
        T_LOGW(TAG, "test mode running: stop send skipped (ID=%s)", device_id_str);
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
        T_LOGW(TAG, "stop command sent: ID=%s", device_id_str);
    } else {
        T_LOGE(TAG, "stop command send failed: %d", ret);
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
        T_LOGI(TAG, "brightness set command sent: ID=%s brightness=%d", device_id_str, brightness);
    } else {
        T_LOGE(TAG, "brightness set command send failed: %d", ret);
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
        T_LOGI(TAG, "camera ID set command sent: ID=%s CameraID=%d", device_id_str, camera_id);
    } else {
        T_LOGE(TAG, "camera ID set command send failed: %d", ret);
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
        T_LOGW(TAG, "reboot command sent: ID=%s", device_id_str);
    } else {
        T_LOGE(TAG, "reboot command send failed: %d", ret);
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
        T_LOGD(TAG, "PING sent: ID=%s, TS=%u, len=%zu",
                 device_id_str, s_ping.timestamp_low, req.length);
    } else {
        T_LOGE(TAG, "PING send failed");
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

    T_LOGI(TAG,
            "Status RX: ID=%s Bat=%d%% Cam=%d Up=%us Freq=%u SW=0x%02X (RSSI:%d SNR:%.1f)",
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
            T_LOGW(TAG, "license device_limit exceeded (%d/%d), stop sent: ID=%s",
                    s_tx.device_count, device_limit, id_str);

            send_stop_command(status->device_id);
            return;
        }

        // MAX_DEVICES 체크
        if (s_tx.device_count >= MAX_DEVICES) {
            T_LOGW(TAG, "device list full (%d)", MAX_DEVICES);
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

    // 기능정지 상태 감지 시 로깅
    if (status->stopped == 1) {
        char device_id_str[5];
        lora_device_id_to_str(status->device_id, device_id_str);
        T_LOGI(TAG, "device in stopped state: ID=%s (will auto-recover on next status request)",
                device_id_str);
    }

    s_tx.devices[found_idx].is_online = true;  // 상태 응답 수신 = 온라인

    // 디바이스-카메라 매핑 이벤트 발행 (ConfigService에서 NVS 저장)
    uint8_t cam_map_data[3] = {status->device_id[0], status->device_id[1], status->camera_id};
    event_bus_publish(EVT_DEVICE_CAM_MAP_RECEIVE, cam_map_data, sizeof(cam_map_data));

    // 디바이스 리스트 변경 이벤트 발행
    device_list_event_t list_event;
    // 버퍼 오버플로우 방지: device_count가 MAX_DEVICES(20) 초과 시 클리핑
    uint8_t copy_count = (s_tx.device_count > MAX_DEVICES) ? MAX_DEVICES : s_tx.device_count;
    memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * copy_count);
    list_event.count = s_tx.device_count;       // 실제 개수는 그대로 전달
    list_event.registered_count = s_tx.device_count;
    event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
}

/**
 * @brief 라이센스 상태 변경 이벤트 핸들러
 */
static esp_err_t on_license_state_changed(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    const license_state_event_t* license = (const license_state_event_t*)event->data;

    s_tx.device_limit = license->device_limit;
    s_tx.limit_valid = true;

    T_LOGI(TAG, "license state changed: limit=%d, state=%d",
            license->device_limit, license->state);

    return ESP_OK;
}

/**
 * @brief RX 응답 이벤트 핸들러 (0xD0~0xDF)
 */
static esp_err_t on_lora_rx_response(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

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
            T_LOGW(TAG, "status response length too short: %d", (int)len);
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
            T_LOGI(TAG, "ACK received: ID=%s Cmd=0x%02X Result=%d",
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
            T_LOGI(TAG, "PONG received: ID=%s Ping=%dms", device_id_str, ping_ms);

            // 해당 디바이스 ping 업데이트
            for (uint8_t i = 0; i < s_tx.device_count; i++) {
                if (lora_device_id_equals(s_tx.devices[i].device_id,
                                          pong->device_id)) {
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
    RETURN_ERR_IF_NULL(event);

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;

    // Tally 패킷(0xF1~0xF4) 전송 시 기록
    if (packet->length > 0) {
        uint8_t header = packet->data[0];
        if (LORA_IS_TALLY_HEADER(header)) {
            s_tx.last_tally_send_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            T_LOGD(TAG, "Tally send record: header=0x%02X", header);
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
 * @brief LED 색상 브로드캐스트 명령 송신 (0xE8)
 */
static esp_err_t send_led_colors_command(const lora_cmd_led_colors_t* colors)
{
    static lora_cmd_led_colors_t s_cmd;

    s_cmd.header = LORA_HDR_LED_COLORS;
    s_cmd.program_r = colors->program_r;
    s_cmd.program_g = colors->program_g;
    s_cmd.program_b = colors->program_b;
    s_cmd.preview_r = colors->preview_r;
    s_cmd.preview_g = colors->preview_g;
    s_cmd.preview_b = colors->preview_b;
    s_cmd.off_r = colors->off_r;
    s_cmd.off_g = colors->off_g;
    s_cmd.off_b = colors->off_b;

    lora_send_request_t req = {
        .data = (const uint8_t*)&s_cmd,
        .length = sizeof(s_cmd)
    };

    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "LED colors broadcast sent: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d)",
                 colors->program_r, colors->program_g, colors->program_b,
                 colors->preview_r, colors->preview_g, colors->preview_b,
                 colors->off_r, colors->off_g, colors->off_b);
    } else {
        T_LOGE(TAG, "LED colors broadcast send failed: %d", ret);
    }

    return ret;
}

/**
 * @brief LED 색상 브로드캐스트 요청 이벤트 핸들러
 */
static esp_err_t on_device_led_colors_request(const event_data_t* event)
{
    if (!event || event->data_size < sizeof(lora_cmd_led_colors_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const lora_cmd_led_colors_t* colors = (const lora_cmd_led_colors_t*)event->data;
    return send_led_colors_command(colors);
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
    T_LOGI(TAG, "test mode start: stop send disabled");
    return ESP_OK;
}

/**
 * @brief 테스트 모드 중지 이벤트 핸들러
 */
static esp_err_t on_test_mode_stop(const event_data_t* event)
{
    (void)event;
    s_test_mode_running = false;
    T_LOGI(TAG, "test mode stop: stop send enabled");
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
            T_LOGI(TAG, "device camera ID load: [%02X%02X] → Cam%d",
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
        T_LOGI(TAG, "device camera ID load (offline): [%02X%02X] → Cam%d",
                device_id[0], device_id[1], camera_id);

        // 웹 서버에 디바이스 리스트 변경 알림
        device_list_event_t list_event;
        memset(&list_event, 0, sizeof(list_event));
        // 버퍼 오버플로우 방지: device_count가 MAX_DEVICES(20) 초과 시 클리핑
        uint8_t copy_count = (s_tx.device_count > MAX_DEVICES) ? MAX_DEVICES : s_tx.device_count;
        memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * copy_count);
        list_event.count = s_tx.device_count;       // 실제 개수는 그대로 전달
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
                memcpy(&s_tx.devices[i], &s_tx.devices[s_tx.device_count],
                       sizeof(device_info_t));
            }

            T_LOGI(TAG, "device removed from list: [%02X%02X]",
                    req->device_id[0], req->device_id[1]);

            // 웹 서버에 디바이스 리스트 변경 알림
            device_list_event_t list_event;
            memset(&list_event, 0, sizeof(list_event));
            // 버퍼 오버플로우 방지: device_count가 MAX_DEVICES(20) 초과 시 클리핑
            uint8_t copy_count = (s_tx.device_count > MAX_DEVICES) ? MAX_DEVICES : s_tx.device_count;
            memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * copy_count);
            list_event.count = s_tx.device_count;       // 실제 개수는 그대로 전달
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
            T_LOGW(TAG, "device offline: ID=%s (no response %u sec)",
                    device_id_str, elapsed / 1000);
            list_changed = true;
        }
    }

    // 상태 변경 시 이벤트 발행
    if (list_changed) {
        device_list_event_t list_event;
        // 버퍼 오버플로우 방지: device_count가 MAX_DEVICES(20) 초과 시 클리핑
        uint8_t copy_count = (s_tx.device_count > MAX_DEVICES) ? MAX_DEVICES : s_tx.device_count;
        memcpy(list_event.devices, s_tx.devices, sizeof(device_info_t) * copy_count);
        list_event.count = s_tx.device_count;       // 실제 개수는 그대로 전달
        list_event.registered_count = s_tx.device_count;
        event_bus_publish(EVT_DEVICE_LIST_CHANGED, &list_event, sizeof(list_event));
    }
}

/**
 * @brief 상태 요청 태스크 (마지막 Tally 전송 후 30초 경과 시)
 */
static void status_request_task(void* arg)
{
    T_LOGI(TAG, "status request task start (%d ms after last Tally)", s_tx.request_interval_ms);

    while (s_mgr.running) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 오프라인 디바이스 체크
        check_offline_devices();

        // 마지막 Tally 전송 후 request_interval_ms 경과 시 상태 요청
        uint32_t elapsed = now - s_tx.last_tally_send_time;

        // 디버깅: 10초마다 경과 시간 로그
        static uint32_t last_log_time = 0;
        if (now - last_log_time >= 10000) {
            T_LOGD(TAG, "Tally elapsed: %u ms (request threshold: %u ms)", elapsed, s_tx.request_interval_ms);
            last_log_time = now;
        }

        if (elapsed >= s_tx.request_interval_ms) {
            send_status_request();
        }

        // 1초마다 체크
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    T_LOGI(TAG, "status request task end");
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
    float rf_frequency;              // RF 주파수 (MHz) - EVT_RF_CHANGED
    uint8_t rf_sync_word;            // RF Sync Word - EVT_RF_CHANGED
    bool rf_valid;                   // RF 설정 유효성
} s_rx = {
    .system = {},
    .lora = {},
    .brightness = 255,  // 기본값
    .camera_id = 1,      // 기본값
    .stopped = false,    // 기본값: 정상
    .system_valid = false,
    .lora_valid = false,
    .rf_frequency = 0.0f,
    .rf_sync_word = 0,
    .rf_valid = false
};

/**
 * @brief 시스템 정보 이벤트 핸들러
 */
static esp_err_t on_info_updated(const event_data_t* event)
{
    if (!s_mgr.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    RETURN_ERR_IF_NULL(event);

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
    RETURN_ERR_IF_NULL(event);

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
    RETURN_ERR_IF_NULL(event);

    const uint8_t* brightness = (const uint8_t*)event->data;
    s_rx.brightness = *brightness;
    T_LOGI(TAG, "brightness changed: %d", s_rx.brightness);

    // 변경 즉시 상태 응답 송신 (TX에 새 값 알림)
    send_status_response();

    return ESP_OK;
}

/**
 * @brief 카메라 ID 변경 이벤트 핸들러
 */
static esp_err_t on_camera_id_changed(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    const uint8_t* camera_id = (const uint8_t*)event->data;
    s_rx.camera_id = *camera_id;
    T_LOGI(TAG, "camera ID changed: %d", s_rx.camera_id);

    // 변경 즉시 상태 응답 송신 (TX에 새 값 알림)
    send_status_response();

    return ESP_OK;
}

/**
 * @brief RF 설정 변경 이벤트 핸들러 (RX)
 */
static esp_err_t on_rf_changed_rx(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    const lora_rf_event_t* rf = (const lora_rf_event_t*)event->data;
    s_rx.rf_frequency = rf->frequency;
    s_rx.rf_sync_word = rf->sync_word;
    s_rx.rf_valid = true;

    T_LOGI(TAG, "RF config updated: %.1f MHz, Sync 0x%02X",
            s_rx.rf_frequency, s_rx.rf_sync_word);

    return ESP_OK;
}

/**
 * @brief 상태 응답 송신 (이벤트 기반)
 */
static esp_err_t send_status_response(void)
{
    if (!s_rx.system_valid) {
        T_LOGW(TAG, "no system info, response send skipped");
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

    // 주파수/SyncWord (EVT_RF_CHANGED 이벤트로 수집, fallback으로 lora.frequency)
    if (s_rx.rf_valid) {
        s_status.frequency = (uint16_t)s_rx.rf_frequency;
        s_status.sync_word = s_rx.rf_sync_word;
    } else {
        s_status.frequency = (uint16_t)s_rx.lora.frequency;
        s_status.sync_word = 0x12;  // fallback
    }

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
        T_LOGI(TAG, "status response sent: ID=%s Bat=%d%% Up=%us Stop=%d",
                device_id_str, s_status.battery, s_status.uptime, s_status.stopped);
    } else {
        T_LOGE(TAG, "status response send failed: %d", ret);
    }

    return ret;
}

/**
 * @brief PONG 응답 송신 (0xD2)
 * @param device_id 대상 디바이스 ID (2바이트)
 * @param tx_timestamp_low PING 송신 시간 하위 2바이트 (ms)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 시스템 정보 없음, ESP_FAIL 송신 실패
 * @note 수신한 timestamp_low를 그대로 회신하여 RTT 계산
 */
static esp_err_t send_pong_response(const uint8_t* device_id, uint16_t tx_timestamp_low)
{
    if (!s_rx.system_valid) {
        T_LOGW(TAG, "no system info, PONG send skipped");
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
        T_LOGI(TAG, "PONG sent: ID=%s, TS=%u", device_id_str, tx_timestamp_low);
    } else {
        T_LOGE(TAG, "PONG send failed: %d", ret);
    }

    return ret;
}

// ============================================================================
// RX 명령 처리 헬퍼 함수
// ============================================================================

/**
 * @brief 자신의 Device ID인지 확인
 * @param target_device_id 확인 대상 디바이스 ID (2바이트)
 * @return true 명령 대상이 맞으면 (Broadcast 또는 자신의 ID), false 대상이 아니면
 */
static bool is_my_device(const uint8_t* target_device_id)
{
    // Broadcast는 항상 true
    if (lora_device_id_is_broadcast(target_device_id)) {
        return true;
    }

    // 자신의 Device ID가 4자리 hex 문자열인지 확인
    if (strlen(s_rx.system.device_id) != 4) {
        return false;
    }

    // 문자열에서 2바이트 ID로 변환
    uint8_t my_id[2];
    char hex_str[3];
    hex_str[0] = s_rx.system.device_id[0];
    hex_str[1] = s_rx.system.device_id[1];
    hex_str[2] = '\0';
    my_id[0] = (uint8_t)strtol(hex_str, NULL, 16);

    hex_str[0] = s_rx.system.device_id[2];
    hex_str[1] = s_rx.system.device_id[3];
    hex_str[2] = '\0';
    my_id[1] = (uint8_t)strtol(hex_str, NULL, 16);

    return lora_device_id_equals(my_id, target_device_id);
}

/**
 * @brief 상태 요청 명령 처리 (0xE0)
 * @param packet LoRa 패킷 이벤트 (RSSI 정보 포함)
 * @note 충돌 방지를 위해 랜덤 지연 후 상태 응답 송신
 */
static void handle_status_request(const lora_packet_event_t* packet)
{
    // 자동 복구: 기능정지 상태에서 상태 요청 수신 시 정상 상태로 복귀
    if (s_rx.stopped) {
        s_rx.stopped = false;
        T_LOGI(TAG, "auto-recovering from stopped state (display/LED restore)");

        bool stopped_val = false;
        event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));
    }

    T_LOGI(TAG, "status request received (RSSI:%d)", packet->rssi);

    // 충돌 방지를 위한 랜덤 지연 (0-1000ms)
    uint32_t delay_ms = esp_random() % 1000;
    T_LOGD(TAG, "status response delay: %u ms", delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    send_status_response();
}

/**
 * @brief 밝기 설정 명령 처리 (0xE1)
 * @param cmd 밝기 설정 명령 구조체 (device_id, brightness)
 * @note 자신의 Device ID인 경우에만 밝기 변경 이벤트 발행
 */
static void handle_brightness_command(const lora_cmd_brightness_t* cmd)
{
    if (!is_my_device(cmd->device_id)) {
        char device_id_str[5];
        lora_device_id_to_str(cmd->device_id, device_id_str);
        T_LOGD(TAG, "brightness command ignored: target ID=%s (not my ID)", device_id_str);
        return;
    }

    char device_id_str[5];
    lora_device_id_to_str(cmd->device_id, device_id_str);
    T_LOGI(TAG, "brightness set received: ID=%s, brightness=%d", device_id_str, cmd->brightness);

    // 밝기 변경 이벤트 발행 (led_service가 구독)
    event_bus_publish(EVT_BRIGHTNESS_CHANGED, &cmd->brightness, sizeof(cmd->brightness));
}

/**
 * @brief 카메라 ID 설정 명령 처리 (0xE2)
 * @param cmd 카메라 ID 설정 명령 구조체 (device_id, camera_id)
 * @note 자신의 Device ID인 경우에만 카메라 ID 변경 이벤트 발행 (NVS 저장)
 */
static void handle_camera_id_command(const lora_cmd_camera_id_t* cmd)
{
    if (!is_my_device(cmd->device_id)) {
        char device_id_str[5];
        lora_device_id_to_str(cmd->device_id, device_id_str);
        T_LOGD(TAG, "camera ID command ignored: target ID=%s (not my ID)", device_id_str);
        return;
    }

    char device_id_str[5];
    lora_device_id_to_str(cmd->device_id, device_id_str);
    T_LOGI(TAG, "camera ID set received: ID=%s, CameraID=%d", device_id_str, cmd->camera_id);

    // 이벤트 발행 (config_service가 NVS 저장)
    event_bus_publish(EVT_CAMERA_ID_CHANGED, &cmd->camera_id, sizeof(cmd->camera_id));
}

/**
 * @brief RF 설정 명령 처리 (0xE3)
 * @param data 패킷 데이터 (header 1바이트 + frequency 4바이트 + sync_word 1바이트)
 * @param len 데이터 길이 (6바이트여야 함)
 * @note RF 변경 이벤트 발행 (lora_service에서 드라이버 적용)
 */
static void handle_rf_command(const uint8_t* data, size_t len)
{
    if (len != 6) {
        T_LOGW(TAG, "RF command length error: %d (expected: 6)", len);
        return;
    }

    float frequency;
    memcpy(&frequency, &data[1], sizeof(float));
    uint8_t sync_word = data[5];

    T_LOGI(TAG, "RF config received: %.1f MHz, Sync 0x%02X", frequency, sync_word);

    // 이벤트 발행 (lora_service가 드라이버 적용)
    lora_rf_event_t rf_event = {
        .frequency = frequency,
        .sync_word = sync_word
    };
    event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));
}

/**
 * @brief 전역 밝기 Broadcast 명령 처리 (0xE7)
 * @param cmd 전역 밝기 설정 명령 구조체 (brightness)
 * @note 모든 디바이스 대상으로 밝기 변경 이벤트 발행
 */
static void handle_brightness_broadcast(const lora_cmd_brightness_broadcast_t* cmd)
{
    T_LOGI(TAG, "global brightness set received (Broadcast): %d", cmd->brightness);

    // 밝기 변경 이벤트 발행 (led_service가 구독)
    event_bus_publish(EVT_BRIGHTNESS_CHANGED, &cmd->brightness, sizeof(cmd->brightness));
}

/**
 * @brief LED 색상 Broadcast 명령 처리 (0xE8)
 * @param cmd LED 색상 설정 명령 구조체
 * @note 모든 디바이스 대상으로 LED 색상 변경 이벤트 발행
 */
static void handle_led_colors_command(const lora_cmd_led_colors_t* cmd)
{
    T_LOGI(TAG, "LED colors broadcast received: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d)",
             cmd->program_r, cmd->program_g, cmd->program_b,
             cmd->preview_r, cmd->preview_g, cmd->preview_b,
             cmd->off_r, cmd->off_g, cmd->off_b);

    // 헤더 제외하고 데이터만 발행 (다른 패킷들과 동일 방식)
    const uint8_t* data_start = (const uint8_t*)&cmd->program_r;
    event_bus_publish(EVT_LED_COLORS_CHANGED, data_start, sizeof(led_colors_event_t));
}

/**
 * @brief 기능 정지 명령 처리 (0xE4)
 * @param cmd 기능 정지 명령 구조체 (device_id)
 * @note 자신의 Device ID인 경우에만 기능 정지 상태로 변경 및 이벤트 발행
 */
static void handle_stop_command(const lora_cmd_stop_t* cmd)
{
    if (!is_my_device(cmd->device_id)) {
        return;
    }

    s_rx.stopped = true;
    char device_id_str[5];
    lora_device_id_to_str(cmd->device_id, device_id_str);
    T_LOGW(TAG, "stop command received: ID=%s, display/LED stopped", device_id_str);

    // 기능 정지 이벤트 발행 (다른 서비스에서 처리)
    bool stopped_val = true;
    event_bus_publish(EVT_STOP_CHANGED, &stopped_val, sizeof(stopped_val));
}

/**
 * @brief 재부팅 명령 처리 (0xE5)
 * @param cmd 재부팅 명령 구조체 (device_id)
 * @note Broadcast 또는 자신의 Device ID인 경우 1초 대기 후 재부팅
 */
static void handle_reboot_command(const lora_cmd_reboot_t* cmd)
{
    bool is_broadcast = (cmd->device_id[0] == 0xFF && cmd->device_id[1] == 0xFF);

    if (!is_broadcast && !is_my_device(cmd->device_id)) {
        return;
    }

    char device_id_str[5];
    lora_device_id_to_str(cmd->device_id, device_id_str);
    if (is_broadcast) {
        T_LOGW(TAG, "broadcast reboot command received, rebooting in 1 sec...");
    } else {
        T_LOGW(TAG, "reboot command received: ID=%s, rebooting in 1 sec...", device_id_str);
    }

    // 1초 대기 후 재부팅
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

/**
 * @brief PING 명령 처리 (0xE6)
 * @param cmd PING 명령 구조체 (device_id, timestamp_low)
 * @note 자신의 Device ID인 경우에만 PONG 응답 송신
 */
static void handle_ping_command(const lora_cmd_ping_t* cmd)
{
    if (!is_my_device(cmd->device_id)) {
        return;
    }

    char device_id_str[5];
    lora_device_id_to_str(cmd->device_id, device_id_str);
    T_LOGI(TAG, "PING received: ID=%s, TS=%u", device_id_str, cmd->timestamp_low);

    // PONG 응답 송신
    send_pong_response(cmd->device_id, cmd->timestamp_low);
}

// ============================================================================
// TX 명령 이벤트 핸들러 (0xE0~0xEF)
// ============================================================================

/**
 * @brief TX 명령 이벤트 핸들러 (0xE0~0xEF)
 */
static esp_err_t on_lora_tx_command(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    const lora_packet_event_t* packet = (const lora_packet_event_t*)event->data;
    const uint8_t* data = packet->data;
    size_t len = packet->length;

    if (len == 0) {
        return ESP_OK;
    }

    uint8_t header = data[0];

    // 명령별 헬퍼 함수로 디스패치
    switch (header) {
        case LORA_HDR_STATUS_REQ:  // 0xE0
            handle_status_request(packet);
            break;

        case LORA_HDR_SET_BRIGHTNESS:  // 0xE1
            if (len >= sizeof(lora_cmd_brightness_t)) {
                handle_brightness_command((const lora_cmd_brightness_t*)data);
            } else {
                T_LOGW(TAG, "brightness command length too short: %d < %zu", len, sizeof(lora_cmd_brightness_t));
            }
            break;

        case LORA_HDR_SET_CAMERA_ID:  // 0xE2
            if (len >= sizeof(lora_cmd_camera_id_t)) {
                handle_camera_id_command((const lora_cmd_camera_id_t*)data);
            } else {
                T_LOGW(TAG, "camera ID command length too short: %d < %zu", len, sizeof(lora_cmd_camera_id_t));
            }
            break;

        case LORA_HDR_SET_RF:  // 0xE3
            handle_rf_command(data, len);
            break;

        case LORA_HDR_BRIGHTNESS_BROADCAST:  // 0xE7
            if (len >= sizeof(lora_cmd_brightness_broadcast_t)) {
                handle_brightness_broadcast((const lora_cmd_brightness_broadcast_t*)data);
            } else {
                T_LOGW(TAG, "global brightness command length too short: %d < %zu", len, sizeof(lora_cmd_brightness_broadcast_t));
            }
            break;

        case LORA_HDR_LED_COLORS:  // 0xE8
            if (len >= sizeof(lora_cmd_led_colors_t)) {
                handle_led_colors_command((const lora_cmd_led_colors_t*)data);
            } else {
                T_LOGW(TAG, "LED colors command length too short: %d < %zu", len, sizeof(lora_cmd_led_colors_t));
            }
            break;

        case LORA_HDR_STOP:  // 0xE4
            if (len >= sizeof(lora_cmd_stop_t)) {
                handle_stop_command((const lora_cmd_stop_t*)data);
            } else {
                T_LOGW(TAG, "stop command length too short: %d < %zu", len, sizeof(lora_cmd_stop_t));
            }
            break;

        case LORA_HDR_REBOOT:  // 0xE5
            if (len >= sizeof(lora_cmd_reboot_t)) {
                handle_reboot_command((const lora_cmd_reboot_t*)data);
            } else {
                T_LOGW(TAG, "reboot command length too short: %d < %zu", len, sizeof(lora_cmd_reboot_t));
            }
            break;

        case LORA_HDR_PING:  // 0xE6
            if (len >= sizeof(lora_cmd_ping_t)) {
                handle_ping_command((const lora_cmd_ping_t*)data);
            } else {
                T_LOGW(TAG, "PING command length too short: %d < %zu", len, sizeof(lora_cmd_ping_t));
            }
            break;

        default:
            T_LOGD(TAG, "TX command received (future implementation): 0x%02X", header);
            break;
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

    T_LOGI(TAG, "initializing...");

    s_mgr.initialized = true;
    s_mgr.running = false;

    // 초기화 완료 후 자동 시작
    device_manager_start();

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
    event_bus_subscribe(EVT_DEVICE_LED_COLORS_REQUEST, on_device_led_colors_request);
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
        8192,  // 스택 크기 증가 (3072 -> 8192) - stack overflow 방지
        nullptr,
        5,  // 우선순위
        nullptr,
        0   // Core 0
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "status request task creation failed");
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
    event_bus_subscribe(EVT_RF_CHANGED, on_rf_changed_rx);

    // TX 명령 이벤트 구독 (0xE0~0xEF)
    event_bus_subscribe(EVT_LORA_TX_COMMAND, on_lora_tx_command);

    // 부팅 직후 상태 응답 송신 (시스템 정보 준비 대기 후)
    // 충돌 방지를 위해 랜덤 지연 (0-2000ms) 후 송신
    uint32_t boot_delay_ms = esp_random() % 2000;
    T_LOGI(TAG, "status response will be sent after %u ms boot delay", boot_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(boot_delay_ms));
    send_status_response();
#endif

    T_LOGI(TAG, "service started");
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
    event_bus_unsubscribe(EVT_DEVICE_LED_COLORS_REQUEST, on_device_led_colors_request);
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
    event_bus_unsubscribe(EVT_RF_CHANGED, on_rf_changed_rx);
    event_bus_unsubscribe(EVT_LORA_TX_COMMAND, on_lora_tx_command);
#endif

    T_LOGI(TAG, "service stopped");
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
    T_LOGI(TAG, "status request interval changed: %d ms", interval_ms);
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
