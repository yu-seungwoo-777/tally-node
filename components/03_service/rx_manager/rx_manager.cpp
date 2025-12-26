/**
 * @file rx_manager.cpp
 * @brief RX 디바이스 관리 서비스 구현 (event_bus 기반 + NVS 저장)
 */

#include "rx_manager.h"
#include "lora_protocol.h"
#include "LoRaService.h"
#include "event_bus.h"
#include "t_log.h"
#include <cstring>
#include <cstdio>

#include "nvs_flash.h"
#include "nvs.h"

static const char* TAG __attribute__((unused)) = "RxManager";

// ============================================================================
// NVS 설정
// ============================================================================

#define NVS_NAMESPACE "rx_mgr"
#define NVS_KEY_COUNT "reg_count"
#define NVS_KEY_PREFIX "dev_"

// ============================================================================
// 내부 상태
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;

// 디바이스 목록 (RAM, 실시간 상태)
static rx_device_t s_devices[RX_MANAGER_MAX_DEVICES];  // init에서 memset로 초기화
static uint8_t s_device_count = 0;

// 등록된 디바이스 (NVS 저장, 영구적)
static uint8_t s_registered_devices[RX_MANAGER_MAX_REGISTERED][LORA_DEVICE_ID_LEN] = {0};
static uint8_t s_registered_count = 0;

// 이벤트 콜백
static rx_manager_event_callback_t s_event_callback = nullptr;

// ============================================================================
// 내부 함수 - 등록된 디바이스 관리
// ============================================================================

/**
 * @brief 등록된 디바이스 목록에서 ID 찾기
 * @return 인덱스 (0~RX_MANAGER_MAX_REGISTERED-1), 못찾으면 -1
 */
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

/**
 * @brief 빈 등록 슬롯 찾기
 */
static int find_empty_registered_slot(void) {
    for (uint8_t i = 0; i < RX_MANAGER_MAX_REGISTERED; i++) {
        if (s_registered_devices[i][0] == 0 &&
            s_registered_devices[i][1] == 0 &&
            s_registered_devices[i][2] == 0 &&
            s_registered_devices[i][3] == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 미등록 디바이스에 STOP 명령 전송
 */
static void send_stop_to_unregistered(const uint8_t* device_id) {
    lora_cmd_stop_t cmd;
    cmd.header = LORA_HDR_STOP;
    memcpy(cmd.device_id, device_id, LORA_DEVICE_ID_LEN);

    esp_err_t ret = lora_service_send(reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
    if (ret == ESP_OK) {
        char id_str[5];
        lora_device_id_to_str(device_id, id_str);
        T_LOGW(TAG, "STOP sent to unregistered device: %s", id_str);
    }
}

// ============================================================================
// 내부 함수 - 디바이스 관리
// ============================================================================

/**
 * @brief 빈 슬롯 찾기
 */
static int find_empty_slot(void) {
    for (uint8_t i = 0; i < RX_MANAGER_MAX_DEVICES; i++) {
        if (!s_devices[i].is_online) {
            return i;
        }
    }
    return -1;
}

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
                if (s_registered_count < RX_MANAGER_MAX_REGISTERED) {
                    // 등록 가능하면 자동 등록
                    esp_err_t ret = rx_manager_register_device(msg->device_id);
                    if (ret == ESP_OK) {
                        char id_str[5];
                        lora_device_id_to_str(msg->device_id, id_str);
                        T_LOGI(TAG, "Auto-registered new device: %s (%d/%d)",
                               id_str, s_registered_count, RX_MANAGER_MAX_REGISTERED);
                    }
                } else {
                    // 등록 불가능하면 STOP 전송
                    char id_str[5];
                    lora_device_id_to_str(msg->device_id, id_str);
                    T_LOGW(TAG, "Unregistered device %s (full, sending STOP)", id_str);
                    send_stop_to_unregistered(msg->device_id);
                    return ESP_OK;
                }
            }

            // 디바이스 찾기 (등록된 디바이스만 s_devices에 추가)
            int idx = rx_manager_find_device(msg->device_id);

            if (idx < 0) {
                // 새 디바이스 - 빈 슬롯 찾기
                idx = find_empty_slot();
                if (idx < 0) {
                    T_LOGW(TAG, "No empty slot for new device");
                    return ESP_OK;
                }

                // 디바이스 등록
                memset(&s_devices[idx], 0, sizeof(rx_device_t));
                memcpy(s_devices[idx].device_id, msg->device_id, LORA_DEVICE_ID_LEN);
                s_devices[idx].is_online = true;
                s_device_count++;
            }

            // 상태 업데이트
            rx_device_t* dev = &s_devices[idx];
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

            // 콜백 호출
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

            // 디바이스 찾기
            int idx = rx_manager_find_device(msg->device_id);
            if (idx >= 0) {
                // 지연시간 계산 (2바이트 timestamp)
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                uint16_t now_low = (uint16_t)(now & 0xFFFF);
                uint16_t tx_low = msg->tx_timestamp_low;

                // 오버플로우 고려
                uint16_t ping_ms;
                if (now_low >= tx_low) {
                    ping_ms = now_low - tx_low;
                } else {
                    ping_ms = now_low + 0x10000 - tx_low;  // 오버플로우 보정
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

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" {

esp_err_t rx_manager_init(void) {
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "RX 매니저 초기화");

    // 디바이스 목록 초기화
    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;

    // 등록된 디바이스 초기화
    memset(s_registered_devices, 0, sizeof(s_registered_devices));
    s_registered_count = 0;

    // NVS에서 등록된 디바이스 로드
    rx_manager_load_registered();

    s_initialized = true;
    return ESP_OK;
}

esp_err_t rx_manager_start(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        return ESP_OK;
    }

    T_LOGI(TAG, "RX 매니저 시작");

    // event_bus 구독
    esp_err_t ret = event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "event_bus 구독 실패");
        return ret;
    }

    s_started = true;
    return ESP_OK;
}

void rx_manager_stop(void) {
    if (!s_started) {
        return;
    }

    T_LOGI(TAG, "RX 매니저 정지");

    // event_bus 구독 취소
    event_bus_unsubscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);

    s_started = false;
}

void rx_manager_process_packet(const uint8_t* data, size_t length) {
    // 레거시 지원 (event_bus 우선)
    (void)data;
    (void)length;
}

uint8_t rx_manager_get_device_count(void) {
    return s_device_count;
}

uint8_t rx_manager_get_devices(rx_device_t* devices) {
    if (devices == nullptr) {
        return 0;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < RX_MANAGER_MAX_DEVICES; i++) {
        if (s_devices[i].is_online) {
            memcpy(&devices[count], &s_devices[i], sizeof(rx_device_t));
            count++;
        }
    }
    return count;
}

int rx_manager_find_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return -1;
    }

    for (uint8_t i = 0; i < RX_MANAGER_MAX_DEVICES; i++) {
        if (s_devices[i].is_online &&
            lora_device_id_equals(s_devices[i].device_id, device_id)) {
            return i;
        }
    }
    return -1;
}

bool rx_manager_get_device_at(uint8_t index, rx_device_t* device) {
    if (index >= RX_MANAGER_MAX_DEVICES || device == nullptr) {
        return false;
    }

    if (!s_devices[index].is_online) {
        return false;
    }

    memcpy(device, &s_devices[index], sizeof(rx_device_t));
    return true;
}

void rx_manager_cleanup_offline(uint32_t timeout_ms) {
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    uint8_t removed = 0;

    for (uint8_t i = 0; i < RX_MANAGER_MAX_DEVICES; i++) {
        if (s_devices[i].is_online) {
            uint32_t elapsed = current_tick - s_devices[i].last_seen;
            if (elapsed > timeout_ticks) {
                char id_str[5];
                lora_device_id_to_str(s_devices[i].device_id, id_str);
                T_LOGI(TAG, "Device offline: %s", id_str);

                s_devices[i].is_online = false;
                memset(&s_devices[i], 0, sizeof(rx_device_t));
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

void rx_manager_set_event_callback(rx_manager_event_callback_t callback) {
    s_event_callback = callback;
}

// ============================================================================
// 등록된 디바이스 관리 API
// ============================================================================

esp_err_t rx_manager_register_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // 이미 등록되어 있는지 확인
    if (find_registered_index(device_id) >= 0) {
        return ESP_OK;
    }

    // 빈 슬롯 찾기
    int slot = find_empty_registered_slot();
    if (slot < 0) {
        return ESP_ERR_NO_MEM;
    }

    // 등록
    memcpy(s_registered_devices[slot], device_id, LORA_DEVICE_ID_LEN);
    s_registered_count++;

    // NVS에 저장
    return rx_manager_save_registered();
}

esp_err_t rx_manager_unregister_device(const uint8_t* device_id) {
    if (device_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_registered_index(device_id);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // 제거 (마지막 항목을 가져와서 덮어쓰기)
    if (idx < s_registered_count - 1) {
        memcpy(s_registered_devices[idx], s_registered_devices[s_registered_count - 1],
               LORA_DEVICE_ID_LEN);
    }

    // 마지막 항목 초기화
    memset(s_registered_devices[s_registered_count - 1], 0, LORA_DEVICE_ID_LEN);
    s_registered_count--;

    // NVS에 저장
    return rx_manager_save_registered();
}

bool rx_manager_is_registered(const uint8_t* device_id) {
    return find_registered_index(device_id) >= 0;
}

uint8_t rx_manager_get_registered_count(void) {
    return s_registered_count;
}

uint8_t rx_manager_get_registered_devices(uint8_t* device_ids) {
    if (device_ids == nullptr) {
        return 0;
    }

    for (uint8_t i = 0; i < s_registered_count; i++) {
        memcpy(&device_ids[i * LORA_DEVICE_ID_LEN], s_registered_devices[i],
               LORA_DEVICE_ID_LEN);
    }
    return s_registered_count;
}

esp_err_t rx_manager_load_registered(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        T_LOGD(TAG, "No saved devices found");
        return ESP_OK;
    }

    // 등록된 디바이스 수 로드
    uint8_t count = 0;
    err = nvs_get_u8(handle, NVS_KEY_COUNT, &count);
    if (err != ESP_OK) {
        nvs_close(handle);
        T_LOGD(TAG, "No device count found");
        return ESP_OK;
    }

    // 최대 개수 제한
    if (count > RX_MANAGER_MAX_REGISTERED) {
        count = RX_MANAGER_MAX_REGISTERED;
    }

    // 각 디바이스 ID 로드
    s_registered_count = 0;
    for (uint8_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, i);

        size_t len = LORA_DEVICE_ID_LEN;
        err = nvs_get_blob(handle, key, s_registered_devices[i], &len);
        if (err == ESP_OK && len == LORA_DEVICE_ID_LEN) {
            s_registered_count++;
        }
    }

    nvs_close(handle);

    if (s_registered_count > 0) {
        T_LOGI(TAG, "Loaded %d registered devices", s_registered_count);
    }

    return ESP_OK;
}

esp_err_t rx_manager_save_registered(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        T_LOGE(TAG, "Failed to open NVS");
        return ESP_FAIL;
    }

    // 등록된 디바이스 수 저장
    nvs_set_u8(handle, NVS_KEY_COUNT, s_registered_count);

    // 각 디바이스 ID 저장
    for (uint8_t i = 0; i < s_registered_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, i);
        nvs_set_blob(handle, key, s_registered_devices[i], LORA_DEVICE_ID_LEN);
    }

    // 커밋
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        T_LOGD(TAG, "Saved %d registered devices", s_registered_count);
    } else {
        T_LOGE(TAG, "Failed to save devices");
    }

    return err;
}

void rx_manager_clear_registered(void) {
    memset(s_registered_devices, 0, sizeof(s_registered_devices));
    s_registered_count = 0;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    T_LOGI(TAG, "Cleared all registered devices");
}

} // extern "C"
