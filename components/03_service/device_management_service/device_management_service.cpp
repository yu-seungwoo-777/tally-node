/**
 * @file device_management_service.cpp
 * @brief 디바이스 관리 서비스 구현 (TX/RX 통합)
 *
 * TX: 명령 송신 + 디바이스 목록 관리
 * RX: 명령 수신 및 실행
 */

#include "device_management_service.h"
#include "lora_protocol.h"
#include "event_bus.h"
#include "t_log.h"
#include <cstring>
#include <cstdio>

#include "freertos/FreeRTOS.h"

// ============================================================================
// 내부 상태
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;

// RX 전용 상태
#ifdef DEVICE_MODE_RX
static bool s_stopped = false;
static uint8_t s_device_id[LORA_DEVICE_ID_LEN] = {0};
static device_mgmt_status_callback_t s_status_cb = nullptr;
#endif

// TX 전용 상태
#ifdef DEVICE_MODE_TX
static device_mgmt_device_t s_devices[DEVICE_MGMT_MAX_DEVICES];
static uint8_t s_device_count = 0;
static uint8_t s_registered_devices[DEVICE_MGMT_MAX_REGISTERED][LORA_DEVICE_ID_LEN] = {0};
static uint8_t s_registered_count = 0;
static device_mgmt_event_callback_t s_event_callback = nullptr;
#endif

// ============================================================================
// 내부 함수 - TX 전용
// ============================================================================

#ifdef DEVICE_MODE_TX

static int find_registered_index(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return -1;
    }
    for (uint8_t i = 0; i < s_registered_count; i++) {
        if (lora_device_id_equals(s_registered_devices[i], device_id)) {
            return i;
        }
    }
    return -1;
}

static int find_empty_slot(void) {
    for (uint8_t i = 0; i < DEVICE_MGMT_MAX_DEVICES; i++) {
        if (!s_devices[i].is_online) {
            return i;
        }
    }
    return -1;
}

static void send_stop_to_unregistered(const uint8_t* device_id) {
    lora_cmd_stop_t cmd;
    cmd.header = LORA_HDR_STOP;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    // event_bus를 통해 LoRa 송신 요청 발행
    lora_send_request_t req = {
        .data = reinterpret_cast<const uint8_t*>(&cmd),
        .length = sizeof(cmd)
    };
    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        char id_str[5];
        lora_device_id_to_str(device_id, id_str);
        T_LOGW(TAG, "STOP sent to unregistered device: %s", id_str);
    }
}

static esp_err_t send_packet(const void* data, size_t length) {
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    // event_bus를 통해 LoRa 송신 요청 발행
    lora_send_request_t req = {
        .data = static_cast<const uint8_t*>(data),
        .length = length
    };
    return event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
}

#endif // DEVICE_MODE_TX

// ============================================================================
// 내부 함수 - RX 전용
// ============================================================================

#ifdef DEVICE_MODE_RX

static void send_ack(uint8_t cmd_header, uint8_t result) {
    lora_msg_ack_t ack;
    ack.header = LORA_HDR_ACK;
    ack.cmd_header = cmd_header;
    ack.result = result;
    memcpy(ack.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    // event_bus를 통해 LoRa 송신 요청 발행
    lora_send_request_t req = {
        .data = reinterpret_cast<const uint8_t*>(&ack),
        .length = sizeof(ack)
    };
    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        T_LOGD(TAG, "ACK sent: cmd=0x%02X, result=%d", cmd_header, result);
    } else {
        T_LOGW(TAG, "ACK send failed: %d", ret);
    }
}

static void send_status(void) {
    if (s_status_cb == nullptr) {
        T_LOGW(TAG, "Status callback not set");
        return;
    }

    device_mgmt_status_t status;
    s_status_cb(&status);

    lora_msg_status_t msg;
    msg.header = LORA_HDR_STATUS;
    msg.battery = status.battery;
    msg.camera_id = status.camera_id;
    msg.uptime = status.uptime;
    msg.brightness = status.brightness;
    msg.flags = s_stopped ? static_cast<uint8_t>(LORA_STATUS_FLAG_STOPPED) : static_cast<uint8_t>(0);
    memcpy(msg.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    // event_bus를 통해 LoRa 송신 요청 발행
    lora_send_request_t req = {
        .data = reinterpret_cast<const uint8_t*>(&msg),
        .length = sizeof(msg)
    };
    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        T_LOGD(TAG, "STATUS sent");
    } else {
        T_LOGW(TAG, "STATUS send failed: %d", ret);
    }
}

static void send_pong(uint16_t tx_timestamp_low) {
    lora_msg_pong_t pong;
    pong.header = LORA_HDR_PONG;
    pong.tx_timestamp_low = tx_timestamp_low;
    memcpy(pong.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    // event_bus를 통해 LoRa 송신 요청 발행
    lora_send_request_t req = {
        .data = reinterpret_cast<const uint8_t*>(&pong),
        .length = sizeof(pong)
    };
    esp_err_t ret = event_bus_publish(EVT_LORA_SEND_REQUEST, &req, sizeof(req));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "  PONG 송신: ts_low=%u", tx_timestamp_low);
    } else {
        T_LOGW(TAG, "PONG send failed: %d", ret);
    }
}

#endif // DEVICE_MODE_RX

// ============================================================================
// LoRa 패킷 수신 처리
// ============================================================================

#ifdef DEVICE_MODE_TX

static esp_err_t on_lora_packet_received(const event_data_t* event) {
    if (event->type != EVT_LORA_PACKET_RECEIVED) {
        return ESP_OK;
    }

    const auto* packet_evt = reinterpret_cast<const lora_packet_event_t*>(event->data);
    const uint8_t* data = packet_evt->data;
    size_t length = packet_evt->length;

    if (length == 0) {
        return ESP_OK;
    }

    uint8_t header = data[0];

    // RX→TX 응답만 처리
    if (!lora_header_is_rx_response(header)) {
        return ESP_OK;
    }

    T_LOGD(TAG, "Packet received: header=0x%02X, len=%d, rssi=%d, snr=%.1f",
           header, length, packet_evt->rssi, packet_evt->snr);

    switch (header) {
        case LORA_HDR_STATUS: {
            if (length < sizeof(lora_msg_status_t)) {
                T_LOGW(TAG, "Invalid STATUS packet");
                return ESP_OK;
            }

            const auto* msg = reinterpret_cast<const lora_msg_status_t*>(data);

            char id_str[5];
            lora_device_id_to_str(msg->device_id, id_str);

            T_LOGD(TAG, "  STATUS: id=%s, bat=%d%%, cam=%d, up=%us, brt=%d%%, flags=0x%02X",
                   id_str, msg->battery, msg->camera_id, msg->uptime,
                   msg->brightness, msg->flags);

            // 등록 여부 확인
            int reg_idx = find_registered_index(msg->device_id);

            if (reg_idx < 0) {
                // 미등록 디바이스
                if (s_registered_count < DEVICE_MGMT_MAX_REGISTERED) {
                    // 자동 등록
                    if (device_mgmt_register_device(msg->device_id) == ESP_OK) {
                        T_LOGI(TAG, "Auto-registered new device: %s (%d/%d)",
                               id_str, s_registered_count, DEVICE_MGMT_MAX_REGISTERED);
                    }
                } else {
                    // 등록 불가능하면 STOP 전송
                    T_LOGW(TAG, "Unregistered device %s (full, sending STOP)", id_str);
                    send_stop_to_unregistered(msg->device_id);
                    return ESP_OK;
                }
            }

            // 디바이스 찾기
            int idx = device_mgmt_find_device(msg->device_id);

            if (idx < 0) {
                // 새 디바이스 - 빈 슬롯 찾기
                idx = find_empty_slot();
                if (idx < 0) {
                    T_LOGW(TAG, "No empty slot for new device");
                    return ESP_OK;
                }

                memset(&s_devices[idx], 0, sizeof(device_mgmt_device_t));
                memcpy(s_devices[idx].device_id, msg->device_id, LORA_DEVICE_ID_LEN);
                s_devices[idx].is_online = true;
                s_device_count++;
            }

            // 상태 업데이트
            device_mgmt_device_t* dev = &s_devices[idx];
            dev->last_rssi = packet_evt->rssi;
            dev->last_snr = packet_evt->snr;
            dev->battery = msg->battery;
            dev->camera_id = msg->camera_id;
            dev->uptime = msg->uptime;
            dev->brightness = msg->brightness;
            dev->is_stopped = (msg->flags & LORA_STATUS_FLAG_STOPPED) != 0;
            dev->last_seen = xTaskGetTickCount();

            T_LOGD(TAG, "Device %d updated: bat=%d%%, cam=%d",
                   idx, dev->battery, dev->camera_id);

            if (s_event_callback) {
                s_event_callback();
            }
            break;
        }

        case LORA_HDR_ACK: {
            if (length < sizeof(lora_msg_ack_t)) {
                return ESP_OK;
            }

            const auto* msg = reinterpret_cast<const lora_msg_ack_t*>(data);

            char id_str[5];
            lora_device_id_to_str(msg->device_id, id_str);
            T_LOGD(TAG, "ACK from %s: cmd=0x%02X, result=%d",
                   id_str, msg->cmd_header, msg->result);
            break;
        }

        case LORA_HDR_PONG: {
            if (length < sizeof(lora_msg_pong_t)) {
                return ESP_OK;
            }

            const auto* msg = reinterpret_cast<const lora_msg_pong_t*>(data);

            char id_str[5];
            lora_device_id_to_str(msg->device_id, id_str);

            int idx = device_mgmt_find_device(msg->device_id);
            if (idx >= 0) {
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                uint16_t now_low = (uint16_t)(now & 0xFFFF);
                uint16_t tx_low = msg->tx_timestamp_low;

                uint16_t ping_ms;
                if (now_low >= tx_low) {
                    ping_ms = now_low - tx_low;
                } else {
                    ping_ms = now_low + 0x10000 - tx_low;
                }

                s_devices[idx].ping_ms = ping_ms;
                s_devices[idx].last_seen = xTaskGetTickCount();

                T_LOGI(TAG, "  PONG 수신: id=%s, tx_low=%u, now_low=%u, ping=%ums",
                       id_str, tx_low, now_low, ping_ms);

                if (s_event_callback) {
                    s_event_callback();
                }
            }
            break;
        }

        default:
            T_LOGW(TAG, "Unknown response: 0x%02X", header);
            break;
    }

    return ESP_OK;
}

#endif // DEVICE_MODE_TX

#ifdef DEVICE_MODE_RX

static esp_err_t on_lora_packet_received(const event_data_t* event) {
    if (event->type != EVT_LORA_PACKET_RECEIVED) {
        return ESP_OK;
    }

    const auto* packet_evt = reinterpret_cast<const lora_packet_event_t*>(event->data);
    const uint8_t* data = packet_evt->data;
    size_t length = packet_evt->length;

    if (length == 0 || !s_started) {
        return ESP_OK;
    }

    uint8_t header = data[0];

    // TX→RX 명령만 처리
    if (!lora_header_is_tx_command(header)) {
        return ESP_OK;
    }

    T_LOGD(TAG, "TX→RX Command: header=0x%02X, len=%d, rssi=%d, snr=%.1f",
           header, length, packet_evt->rssi, packet_evt->snr);

    switch (header) {
        case LORA_HDR_STATUS_REQ: {
            T_LOGI(TAG, "STATUS_REQ 수신");
            send_status();
            break;
        }

        case LORA_HDR_SET_BRIGHTNESS: {
            if (length < sizeof(lora_cmd_brightness_t)) {
                T_LOGW(TAG, "Invalid SET_BRIGHTNESS packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_brightness_t*>(data);

            char id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);

            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  SET_BRIGHTNESS: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "SET_BRIGHTNESS 수신");
            T_LOGD(TAG, "  id=%s, brightness=%d%%", id_str, cmd->brightness);

            // event_bus로 밝기 변경 이벤트 발행 (0-100 → 0-255 변환)
            uint8_t brightness_255 = (cmd->brightness * 255) / 100;
            event_bus_publish(EVT_BRIGHTNESS_CHANGED, &brightness_255, sizeof(brightness_255));

            send_ack(LORA_HDR_SET_BRIGHTNESS, LORA_ACK_SUCCESS);
            break;
        }

        case LORA_HDR_SET_CAMERA_ID: {
            if (length < sizeof(lora_cmd_camera_id_t)) {
                T_LOGW(TAG, "Invalid SET_CAMERA_ID packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_camera_id_t*>(data);

            char id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);

            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  SET_CAMERA_ID: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "SET_CAMERA_ID 수신");
            T_LOGD(TAG, "  id=%s, camera_id=%d", id_str, cmd->camera_id);

            // event_bus로 카메라 ID 변경 이벤트 발행
            event_bus_publish(EVT_CAMERA_ID_CHANGED, &cmd->camera_id, sizeof(cmd->camera_id));

            send_ack(LORA_HDR_SET_CAMERA_ID, LORA_ACK_SUCCESS);
            break;
        }

        case LORA_HDR_SET_RF: {
            if (length < sizeof(lora_cmd_rf_t)) {
                T_LOGW(TAG, "Invalid SET_RF packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_rf_t*>(data);

            char id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);

            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  SET_RF: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "SET_RF 수신");
            T_LOGD(TAG, "  id=%s, freq=%.1fMHz, sync=0x%02X",
                   id_str, cmd->frequency, cmd->sync_word);

            // event_bus로 RF 설정 변경 이벤트 발행
            lora_rf_event_t rf_event = {
                .frequency = cmd->frequency,
                .sync_word = cmd->sync_word
            };
            event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));

            send_ack(LORA_HDR_SET_RF, LORA_ACK_SUCCESS);
            break;
        }

        case LORA_HDR_STOP: {
            if (length < sizeof(lora_cmd_stop_t)) {
                T_LOGW(TAG, "Invalid STOP packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_stop_t*>(data);

            char id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);

            if (!lora_device_id_is_broadcast(cmd->device_id) &&
                !lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  STOP: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "STOP 수신");
            T_LOGD(TAG, "  id=%s", id_str);
            s_stopped = true;

            // event_bus로 정지 상태 변경 이벤트 발행
            bool stopped = true;
            event_bus_publish(EVT_STOP_CHANGED, &stopped, sizeof(stopped));

            send_ack(LORA_HDR_STOP, LORA_ACK_SUCCESS);
            break;
        }

        case LORA_HDR_REBOOT: {
            if (length < sizeof(lora_cmd_reboot_t)) {
                T_LOGW(TAG, "Invalid REBOOT packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_reboot_t*>(data);

            char id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);

            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  REBOOT: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "REBOOT 수신");
            T_LOGD(TAG, "  id=%s", id_str);

            send_ack(LORA_HDR_REBOOT, LORA_ACK_SUCCESS);

            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }

        case LORA_HDR_PING: {
            if (length < sizeof(lora_cmd_ping_t)) {
                T_LOGW(TAG, "Invalid PING packet");
                return ESP_OK;
            }

            const auto* cmd = reinterpret_cast<const lora_cmd_ping_t*>(data);

            char id_str[5];
            char my_id_str[5];
            lora_device_id_to_str(cmd->device_id, id_str);
            lora_device_id_to_str(s_device_id, my_id_str);

            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "PING 무시: 대상이 아님 (target=%s, my_id=%s)", id_str, my_id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "PING 수신");
            T_LOGD(TAG, "  ts_low=%u", cmd->timestamp_low);
            send_pong(cmd->timestamp_low);
            break;
        }

        default:
            T_LOGW(TAG, "Unknown command: 0x%02X", header);
            break;
    }

    return ESP_OK;
}

#endif // DEVICE_MODE_RX

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

static const char* TAG = "DeviceMgmt";

esp_err_t device_management_service_init(device_mgmt_status_callback_t status_cb) {
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

#ifdef DEVICE_MODE_TX
    T_LOGI(TAG, "Device Management Service 초기화 (TX)");
    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;
    memset(s_registered_devices, 0, sizeof(s_registered_devices));
    s_registered_count = 0;
    device_mgmt_load_registered();
#else
    T_LOGI(TAG, "Device Management Service 초기화 (RX)");
    s_status_cb = status_cb;
#endif

    s_initialized = true;
    return ESP_OK;
}

esp_err_t device_management_service_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        return ESP_OK;
    }

    T_LOGI(TAG, "Device Management Service 시작");

#ifdef DEVICE_MODE_TX
    T_LOGI(TAG, "  TX 모드: 명령 송신 + 디바이스 관리");
#else
    T_LOGI(TAG, "  RX 모드: 명령 수신/실행");
#endif

    esp_err_t ret = event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "event_bus 구독 실패");
        return ret;
    }

#ifdef DEVICE_MODE_RX
    s_stopped = false;
#endif

    s_started = true;
    return ESP_OK;
}

void device_management_service_stop(void) {
    if (!s_started) {
        return;
    }

    T_LOGI(TAG, "Device Management Service 정지");

    event_bus_unsubscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    s_started = false;
}

// ============================================================================
// TX 전용 API: 명령 송신
// ============================================================================

#ifdef DEVICE_MODE_TX

esp_err_t device_mgmt_send_status_req(void) {
    uint8_t header = LORA_HDR_STATUS_REQ;
    return send_packet(&header, 1);
}

esp_err_t device_mgmt_set_brightness(const uint8_t* device_id, uint8_t brightness) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_brightness_t cmd;
    cmd.header = LORA_HDR_SET_BRIGHTNESS;
    cmd.brightness = brightness;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGI(TAG, "SET_BRIGHTNESS: id=%s, val=%d", id_str, brightness);

    return send_packet(&cmd, sizeof(cmd));
}

esp_err_t device_mgmt_set_camera_id(const uint8_t* device_id, uint8_t camera_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_camera_id_t cmd;
    cmd.header = LORA_HDR_SET_CAMERA_ID;
    cmd.camera_id = camera_id;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGI(TAG, "SET_CAMERA_ID: id=%s, val=%d", id_str, camera_id);

    return send_packet(&cmd, sizeof(cmd));
}

esp_err_t device_mgmt_set_rf(const uint8_t* device_id, float frequency, uint8_t sync_word) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_rf_t cmd;
    cmd.header = LORA_HDR_SET_RF;
    cmd.frequency = frequency;
    cmd.sync_word = sync_word;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGI(TAG, "SET_RF: id=%s, freq=%.1f, sync=0x%02X", id_str, frequency, sync_word);

    return send_packet(&cmd, sizeof(cmd));
}

esp_err_t device_mgmt_send_stop(const uint8_t* device_id) {
    lora_cmd_stop_t cmd;
    cmd.header = LORA_HDR_STOP;

    if (device_id == nullptr) {
        const uint8_t broadcast_id[LORA_DEVICE_ID_LEN] = {0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(cmd.device_id, broadcast_id, LORA_DEVICE_ID_LEN);
        T_LOGI(TAG, "STOP: broadcast");
    } else {
        char id_str[5];
        lora_device_id_to_str(device_id, id_str);
        memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);
        T_LOGI(TAG, "STOP: id=%s", id_str);
    }

    return send_packet(&cmd, sizeof(cmd));
}

esp_err_t device_mgmt_reboot(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_reboot_t cmd;
    cmd.header = LORA_HDR_REBOOT;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGI(TAG, "REBOOT: id=%s", id_str);

    return send_packet(&cmd, sizeof(cmd));
}

esp_err_t device_mgmt_ping(const uint8_t* device_id, uint32_t timestamp) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_ping_t cmd;
    cmd.header = LORA_HDR_PING;
    cmd.timestamp_low = (uint16_t)(timestamp & 0xFFFF);
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGD(TAG, "PING: id=%s, ts=%u (low=%u)", id_str, timestamp, cmd.timestamp_low);

    return send_packet(&cmd, sizeof(cmd));
}

// ============================================================================
// TX 전용 API: 디바이스 관리
// ============================================================================

uint8_t device_mgmt_get_device_count(void) {
    return s_device_count;
}

uint8_t device_mgmt_get_devices(device_mgmt_device_t* devices) {
    if (devices == nullptr) {
        return 0;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < DEVICE_MGMT_MAX_DEVICES; i++) {
        if (s_devices[i].is_online) {
            memcpy(&devices[count], &s_devices[i], sizeof(device_mgmt_device_t));
            count++;
        }
    }
    return count;
}

int device_mgmt_find_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return -1;
    }

    for (uint8_t i = 0; i < DEVICE_MGMT_MAX_DEVICES; i++) {
        if (s_devices[i].is_online &&
            lora_device_id_equals(s_devices[i].device_id, device_id)) {
            return i;
        }
    }
    return -1;
}

bool device_mgmt_get_device_at(uint8_t index, device_mgmt_device_t* device) {
    if (index >= DEVICE_MGMT_MAX_DEVICES || device == nullptr) {
        return false;
    }

    if (!s_devices[index].is_online) {
        return false;
    }

    memcpy(device, &s_devices[index], sizeof(device_mgmt_device_t));
    return true;
}

void device_mgmt_cleanup_offline(uint32_t timeout_ms) {
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    uint8_t removed = 0;

    for (uint8_t i = 0; i < DEVICE_MGMT_MAX_DEVICES; i++) {
        if (s_devices[i].is_online) {
            uint32_t elapsed = current_tick - s_devices[i].last_seen;
            if (elapsed > timeout_ticks) {
                char id_str[5];
                lora_device_id_to_str(s_devices[i].device_id, id_str);
                T_LOGI(TAG, "Device offline: %s", id_str);

                s_devices[i].is_online = false;
                memset(&s_devices[i], 0, sizeof(device_mgmt_device_t));
                removed++;
            }
        }
    }

    if (removed > 0) {
        s_device_count -= removed;
        if (s_event_callback) {
            s_event_callback();
        }
    }
}

void device_mgmt_set_event_callback(device_mgmt_event_callback_t callback) {
    s_event_callback = callback;
}

// ============================================================================
// TX 전용 API: 등록된 디바이스 관리 (event_bus 사용)
// ============================================================================

esp_err_t device_mgmt_register_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // event_bus를 통해 디바이스 등록 이벤트 발행
    device_register_event_t event;
    memcpy(event.device_id, device_id, LORA_DEVICE_ID_LEN);
    esp_err_t ret = event_bus_publish(EVT_DEVICE_REGISTER, &event, sizeof(event));

    if (ret == ESP_OK) {
        // 메모리 캐시 업데이트
        if (find_registered_index(device_id) < 0) {
            if (s_registered_count < DEVICE_MGMT_MAX_REGISTERED) {
                memcpy(s_registered_devices[s_registered_count], device_id, LORA_DEVICE_ID_LEN);
                s_registered_count++;
            }
        }
    }

    return ret;
}

esp_err_t device_mgmt_unregister_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_registered_index(device_id);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // event_bus를 통해 디바이스 등록 해제 이벤트 발행
    device_register_event_t event;
    memcpy(event.device_id, device_id, LORA_DEVICE_ID_LEN);
    esp_err_t ret = event_bus_publish(EVT_DEVICE_UNREGISTER, &event, sizeof(event));

    if (ret == ESP_OK) {
        // 메모리 캐시 업데이트
        if (idx < s_registered_count - 1) {
            memcpy(s_registered_devices[idx], s_registered_devices[s_registered_count - 1],
                   LORA_DEVICE_ID_LEN);
        }

        memset(s_registered_devices[s_registered_count - 1], 0, LORA_DEVICE_ID_LEN);
        s_registered_count--;
    }

    return ret;
}

bool device_mgmt_is_registered(const uint8_t* device_id) {
    // 메모리 캐시에서 확인
    return find_registered_index(device_id) >= 0;
}

uint8_t device_mgmt_get_registered_count(void) {
    // 메모리 캐시에서 가져오기
    return s_registered_count;
}

uint8_t device_mgmt_get_registered_devices(uint8_t* device_ids) {
    if (device_ids == nullptr) {
        return 0;
    }

    // 메모리 캐시에서 가져오기
    for (uint8_t i = 0; i < s_registered_count; i++) {
        memcpy(&device_ids[i * LORA_DEVICE_ID_LEN], s_registered_devices[i],
               LORA_DEVICE_ID_LEN);
    }
    return s_registered_count;
}

esp_err_t device_mgmt_load_registered(void) {
    // NVS에서 로드하는 대신 빈 함수로 유지
    // 등록된 디바이스는 이벤트를 통해 ConfigService가 관리
    T_LOGD(TAG, "device_mgmt_load_registered: event_bus 기반으로 변경됨");
    return ESP_OK;
}

esp_err_t device_mgmt_save_registered(void) {
    // event_bus를 통해 저장하므로 별도 저장 불필요
    return ESP_OK;
}

void device_mgmt_clear_registered(void) {
    // 모든 등록된 디바이스를 해제 이벤트 발행
    for (int i = s_registered_count - 1; i >= 0; i--) {
        device_register_event_t event;
        memcpy(event.device_id, s_registered_devices[i], LORA_DEVICE_ID_LEN);
        event_bus_publish(EVT_DEVICE_UNREGISTER, &event, sizeof(event));
    }

    // 메모리 캐시 초기화
    memset(s_registered_devices, 0, sizeof(s_registered_devices));
    s_registered_count = 0;

    T_LOGI(TAG, "Cleared all registered devices");
}

#endif // DEVICE_MODE_TX

// ============================================================================
// RX 전용 API: Device ID 관리
// ============================================================================

#ifdef DEVICE_MODE_RX

void device_mgmt_set_device_id(const uint8_t* device_id) {
    if (device_id != nullptr) {
        memcpy(s_device_id, device_id, LORA_DEVICE_ID_LEN);

        char id_str[5];
        lora_device_id_to_str(s_device_id, id_str);
        T_LOGI(TAG, "Device ID set: %s", id_str);
    }
}

const uint8_t* device_mgmt_get_device_id(void) {
    return s_device_id;
}

#endif // DEVICE_MODE_RX

} // extern "C"
