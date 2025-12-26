/**
 * @file rx_command.cpp
 * @brief RX 명령 수신 및 실행 서비스 구현 (event_bus 기반)
 */

#include "rx_command.h"
#include "lora_protocol.h"
#include "LoRaService.h"
#include "event_bus.h"
#include "t_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG __attribute__((unused)) = "RxCommand";

// ============================================================================
// 내부 상태
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;
static bool s_stopped = false;  // 기능 정지 상태

// Device ID (MAC 뒤 4자리)
static uint8_t s_device_id[LORA_DEVICE_ID_LEN] = {0};

// 상태 콜백 (STATUS 전송용)
static rx_command_get_status_callback_t s_get_status_cb = nullptr;

// ============================================================================
// 내부 함수 전방 선언
// ============================================================================

static void send_ack(uint8_t cmd_header, uint8_t result);
static void send_status(void);
static void send_pong(uint16_t tx_timestamp_low);

// ============================================================================
// 내부 함수 구현
// ============================================================================

/**
 * @brief LoRa 패킷 수신 처리 (event_bus 콜백)
 */
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
        T_LOGI(TAG, "RX 패킷 무시: header=0x%02X (TX 명령 아님)", header);
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

            // Device ID 확인
            if (!lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  SET_BRIGHTNESS: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "SET_BRIGHTNESS 수신");
            T_LOGD(TAG, "  id=%s, brightness=%d%%", id_str, cmd->brightness);

            // TODO: 실제 밝기 설정 적용
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

            // TODO: 실제 카메라 ID 설정 적용
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

            // TODO: 실제 RF 설정 적용
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

            // Broadcast 또는 자기 ID 확인
            if (!lora_device_id_is_broadcast(cmd->device_id) &&
                !lora_device_id_equals(cmd->device_id, s_device_id)) {
                T_LOGD(TAG, "  STOP: not for me (target=%s)", id_str);
                return ESP_OK;
            }

            T_LOGI(TAG, "STOP 수신");
            T_LOGD(TAG, "  id=%s", id_str);
            s_stopped = true;

            // TODO: 실제 기능 정지 적용
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

            // ACK 전송 후 잠시 대기 후 재부팅
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

/**
 * @brief ACK 응답 전송
 */
static void send_ack(uint8_t cmd_header, uint8_t result) {
    lora_msg_ack_t ack;
    ack.header = LORA_HDR_ACK;
    ack.cmd_header = cmd_header;
    ack.result = result;
    memcpy(ack.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    esp_err_t ret = lora_service_send(reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
    if (ret == ESP_OK) {
        T_LOGD(TAG, "ACK sent: cmd=0x%02X, result=%d", cmd_header, result);
    } else {
        T_LOGW(TAG, "ACK send failed: %d", ret);
    }
}

/**
 * @brief 상태 응답 전송
 */
static void send_status(void) {
    if (s_get_status_cb == nullptr) {
        T_LOGW(TAG, "Status callback not set");
        return;
    }

    rx_status_t status;
    s_get_status_cb(&status);

    lora_msg_status_t msg;
    msg.header = LORA_HDR_STATUS;
    msg.battery = status.battery;
    msg.camera_id = status.camera_id;
    msg.uptime = status.uptime;
    msg.brightness = status.brightness;
    msg.flags = s_stopped ? static_cast<uint8_t>(LORA_STATUS_FLAG_STOPPED) : static_cast<uint8_t>(0);
    memcpy(msg.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    esp_err_t ret = lora_service_send(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    if (ret == ESP_OK) {
        T_LOGD(TAG, "STATUS sent");
    } else {
        T_LOGW(TAG, "STATUS send failed: %d", ret);
    }
}

/**
 * @brief PONG 응답 전송
 * @param tx_timestamp_low PING의 timestamp 하위 2바이트
 */
static void send_pong(uint16_t tx_timestamp_low) {
    lora_msg_pong_t pong;
    pong.header = LORA_HDR_PONG;
    pong.tx_timestamp_low = tx_timestamp_low;  // 받은 그대로 반환
    memcpy(pong.device_id, s_device_id, LORA_DEVICE_ID_LEN);

    esp_err_t ret = lora_service_send(reinterpret_cast<const uint8_t*>(&pong), sizeof(pong));
    if (ret == ESP_OK) {
        T_LOGI(TAG, "  PONG 송신: ts_low=%u", tx_timestamp_low);
    } else {
        T_LOGW(TAG, "PONG send failed: %d", ret);
    }
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

esp_err_t rx_command_init(rx_command_get_status_callback_t get_status_cb) {
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "RX 명령 서비스 초기화");

    s_get_status_cb = get_status_cb;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t rx_command_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        return ESP_OK;
    }

    T_LOGI(TAG, "RX 명령 서비스 시작");

    // event_bus 구독
    esp_err_t ret = event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "event_bus 구독 실패");
        return ret;
    }

    s_started = true;
    s_stopped = false;

    return ESP_OK;
}

void rx_command_stop(void) {
    if (!s_started) {
        return;
    }

    T_LOGI(TAG, "RX 명령 서비스 정지");

    // event_bus 구독 취소
    event_bus_unsubscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);

    s_started = false;
}

void rx_command_process_packet(const uint8_t* data, size_t length) {
    // 레거시 지원 (event_bus 우선)
    (void)data;
    (void)length;
}

void rx_command_set_device_id(const uint8_t* device_id) {
    if (device_id != nullptr) {
        memcpy(s_device_id, device_id, LORA_DEVICE_ID_LEN);

        char id_str[5];
        lora_device_id_to_str(s_device_id, id_str);
        T_LOGI(TAG, "Device ID set: %s", id_str);
    }
}

const uint8_t* rx_command_get_device_id(void) {
    return s_device_id;
}

} // extern "C"
