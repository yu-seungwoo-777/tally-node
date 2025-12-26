/**
 * @file tx_command.cpp
 * @brief TX 명령 송신 서비스 구현
 */

#include "tx_command.h"
#include "lora_protocol.h"
#include "LoRaService.h"
#include "t_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "TxCommand";

// ============================================================================
// 내부 상태
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 패킷 전송
 */
static esp_err_t send_packet(const void* data, size_t length) {
    if (!s_started) {
        T_LOGW(TAG, "Service not started");
        return ESP_ERR_INVALID_STATE;
    }

    return lora_service_send(static_cast<const uint8_t*>(data), length);
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

esp_err_t tx_command_init(void) {
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "TX 명령 서비스 초기화");
    s_initialized = true;
    return ESP_OK;
}

esp_err_t tx_command_start(void) {
    (void)TAG;  // 경고 방지

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        return ESP_OK;
    }

    T_LOGI(TAG, "TX 명령 서비스 시작");
    s_started = true;
    return ESP_OK;
}

void tx_command_stop(void) {
    if (!s_started) {
        return;
    }

    T_LOGI(TAG, "TX 명령 서비스 정지");
    s_started = false;
}

esp_err_t tx_command_send_status_req(void) {
    uint8_t header = LORA_HDR_STATUS_REQ;
    return send_packet(&header, 1);
}

esp_err_t tx_command_set_brightness(const uint8_t* device_id, uint8_t brightness) {
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

esp_err_t tx_command_set_camera_id(const uint8_t* device_id, uint8_t camera_id) {
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

esp_err_t tx_command_set_rf(const uint8_t* device_id, float frequency, uint8_t sync_word) {
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

esp_err_t tx_command_send_stop(const uint8_t* device_id) {
    lora_cmd_stop_t cmd;
    cmd.header = LORA_HDR_STOP;

    if (device_id == nullptr) {
        // Broadcast
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

esp_err_t tx_command_reboot(const uint8_t* device_id) {
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

esp_err_t tx_command_ping(const uint8_t* device_id, uint32_t timestamp) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lora_cmd_ping_t cmd;
    cmd.header = LORA_HDR_PING;
    cmd.timestamp_low = (uint16_t)(timestamp & 0xFFFF);  // 하위 2바이트만
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    char id_str[5];
    lora_device_id_to_str(device_id, id_str);
    T_LOGD(TAG, "PING: id=%s, ts=%u (low=%u)", id_str, timestamp, cmd.timestamp_low);

    return send_packet(&cmd, sizeof(cmd));
}

} // extern "C"
