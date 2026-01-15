/**
 * @file api_devices.cpp
 * @brief API Devices 핸들러 구현
 */

#include "api_devices.h"
#include "web_server_helpers.h"
#include "web_server_cache.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "t_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "02_WebSvr_Devices";

extern "C" {

esp_err_t api_devices_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    const web_server_data_t* cache = web_server_cache_get();

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 디바이스 수
    cJSON_AddNumberToObject(root, "count", cache->devices.count);
    cJSON_AddNumberToObject(root, "registeredCount", cache->devices.registered_count);

    // 디바이스 배열
    cJSON* devices_array = cJSON_CreateArray();
    if (devices_array) {
        for (uint8_t i = 0; i < cache->devices.count; i++) {
            const device_info_t* dev = &cache->devices.devices[i];

            cJSON* item = cJSON_CreateObject();
            if (item) {
                // Device ID (hex 문자열) - 2바이트
                char id_str[5];
                snprintf(id_str, sizeof(id_str), "%02X%02X",
                         dev->device_id[0], dev->device_id[1]);
                cJSON_AddStringToObject(item, "id", id_str);

                // RSSI, SNR
                cJSON_AddNumberToObject(item, "rssi", dev->last_rssi);
                cJSON_AddNumberToObject(item, "snr", dev->last_snr);

                // 배터리
                cJSON_AddNumberToObject(item, "battery", dev->battery);

                // 카메라 ID
                cJSON_AddNumberToObject(item, "cameraId", dev->camera_id);

                // 업타임
                cJSON_AddNumberToObject(item, "uptime", dev->uptime);

                // 상태 플래그
                cJSON_AddBoolToObject(item, "stopped", dev->is_stopped);
                cJSON_AddBoolToObject(item, "is_online", dev->is_online);

                // Ping
                cJSON_AddNumberToObject(item, "ping", dev->ping_ms);

                // 밝기 (0-255 → 0-100 변환)
                uint8_t brightness_percent = (dev->brightness * 100) / 255;
                cJSON_AddNumberToObject(item, "brightness", brightness_percent);

                // RF 설정
                cJSON_AddNumberToObject(item, "frequency", dev->frequency);
                cJSON_AddNumberToObject(item, "syncWord", dev->sync_word);

                cJSON_AddItemToArray(devices_array, item);
            }
        }
        cJSON_AddItemToObject(root, "devices", devices_array);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json_str);
        free(json_str);
    } else {
        httpd_resp_send_500(req);
    }

    return ESP_OK;
}

esp_err_t api_delete_device_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // deviceId 추출 (배열 형태: [0x2D, 0x78])
    cJSON* device_id_json = cJSON_GetObjectItem(root, "deviceId");
    if (!device_id_json || !cJSON_IsArray(device_id_json)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Missing or invalid 'deviceId' field\"}");
        return ESP_OK;
    }

    // 디바이스 ID 파싱
    uint8_t device_id[2] = {0};
    cJSON* item0 = cJSON_GetArrayItem(device_id_json, 0);
    cJSON* item1 = cJSON_GetArrayItem(device_id_json, 1);
    if (item0 && cJSON_IsNumber(item0)) {
        device_id[0] = (uint8_t)item0->valueint;
    }
    if (item1 && cJSON_IsNumber(item1)) {
        device_id[1] = (uint8_t)item1->valueint;
    }

    cJSON_Delete(root);

    // 디바이스 등록 해제 이벤트 발행
    device_register_event_t unregister_event;
    memcpy(unregister_event.device_id, device_id, 2);
    event_bus_publish(EVT_DEVICE_UNREGISTER, &unregister_event, sizeof(unregister_event));

    char id_str[5];
    snprintf(id_str, sizeof(id_str), "%02X%02X", device_id[0], device_id[1]);
    T_LOGI(TAG, "Device delete request: %s", id_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_device_brightness_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* deviceId_json = cJSON_GetObjectItem(root, "deviceId");
    cJSON* brightness_json = cJSON_GetObjectItem(root, "brightness");

    if (!deviceId_json || !brightness_json) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"deviceId and brightness are required\"}");
        return ESP_OK;
    }

    // deviceId 배열 파싱
    uint8_t device_id[2] = {0xFF, 0xFF};  // 기본 broadcast
    if (cJSON_IsArray(deviceId_json) && cJSON_GetArraySize(deviceId_json) >= 2) {
        device_id[0] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 0)->valueint;
        device_id[1] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 1)->valueint;
    }

    uint8_t brightness = (uint8_t)brightness_json->valueint;

    cJSON_Delete(root);

    // 밝기 변경 이벤트 발행 (lora_service가 구독하여 LoRa 전송)
    // 이벤트 데이터: [device_id[0], device_id[1], brightness]
    uint8_t event_data[3] = {device_id[0], device_id[1], brightness};
    event_bus_publish(EVT_DEVICE_BRIGHTNESS_REQUEST, event_data, sizeof(event_data));

    T_LOGD(TAG, "Device brightness request: ID[%02X%02X], brightness=%d",
             device_id[0], device_id[1], brightness);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_device_camera_id_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* deviceId_json = cJSON_GetObjectItem(root, "deviceId");
    cJSON* cameraId_json = cJSON_GetObjectItem(root, "cameraId");

    if (!deviceId_json || !cameraId_json) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"deviceId and cameraId are required\"}");
        return ESP_OK;
    }

    // deviceId 배열 파싱
    uint8_t device_id[2] = {0xFF, 0xFF};  // 기본 broadcast
    if (cJSON_IsArray(deviceId_json) && cJSON_GetArraySize(deviceId_json) >= 2) {
        device_id[0] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 0)->valueint;
        device_id[1] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 1)->valueint;
    }

    uint8_t camera_id = (uint8_t)cameraId_json->valueint;

    cJSON_Delete(root);

    // 카메라 ID 변경 이벤트 발행 (lora_service가 구독하여 LoRa 전송)
    // 이벤트 데이터: [device_id[0], device_id[1], camera_id]
    uint8_t event_data[3] = {device_id[0], device_id[1], camera_id};
    event_bus_publish(EVT_DEVICE_CAMERA_ID_REQUEST, event_data, sizeof(event_data));

    T_LOGD(TAG, "Device camera ID request: ID[%02X%02X], CameraID=%d",
             device_id[0], device_id[1], camera_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

#ifdef DEVICE_MODE_TX

esp_err_t api_brightness_broadcast_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* brightness_json = cJSON_GetObjectItem(root, "brightness");
    if (!brightness_json || !cJSON_IsNumber(brightness_json)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"brightness required\"}");
        return ESP_OK;
    }

    int brightness = brightness_json->valueint;
    if (brightness < 0 || brightness > 255) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"brightness must be 0-255\"}");
        return ESP_OK;
    }

    cJSON_Delete(root);

    T_LOGD(TAG, "Broadcast brightness control request: brightness=%d", brightness);

    // 전역 밝기 Broadcast 명령 패킷 생성 (0xE7, device_id 없음)
    static lora_cmd_brightness_broadcast_t cmd;
    cmd.header = LORA_HDR_BRIGHTNESS_BROADCAST;
    cmd.brightness = (uint8_t)brightness;

    // LoRa 송신 요청 이벤트 발행
    lora_send_request_t send_req = {
        .data = (const uint8_t*)&cmd,
        .length = sizeof(cmd)
    };
    event_bus_publish(EVT_LORA_SEND_REQUEST, &send_req, sizeof(send_req));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_device_ping_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* deviceId_json = cJSON_GetObjectItem(root, "deviceId");

    // deviceId 배열 파싱
    uint8_t device_id[2] = {0xFF, 0xFF};  // 기본 broadcast
    if (deviceId_json && cJSON_IsArray(deviceId_json) && cJSON_GetArraySize(deviceId_json) >= 2) {
        device_id[0] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 0)->valueint;
        device_id[1] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 1)->valueint;
    }

    cJSON_Delete(root);

    // PING 요청 이벤트 발행 (device_manager가 구독하여 LoRa 전송)
    event_bus_publish(EVT_DEVICE_PING_REQUEST, device_id, sizeof(device_id));

    T_LOGD(TAG, "Device PING request: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_device_stop_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* deviceId_json = cJSON_GetObjectItem(root, "deviceId");

    // deviceId 배열 파싱
    uint8_t device_id[2] = {0xFF, 0xFF};  // 기본 broadcast
    if (deviceId_json && cJSON_IsArray(deviceId_json) && cJSON_GetArraySize(deviceId_json) >= 2) {
        device_id[0] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 0)->valueint;
        device_id[1] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 1)->valueint;
    }

    cJSON_Delete(root);

    // STOP 요청 이벤트 발행
    event_bus_publish(EVT_DEVICE_STOP_REQUEST, device_id, sizeof(device_id));

    T_LOGD(TAG, "Device stop request: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_device_reboot_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 바디 읽기
    char* buf = new char[256];
    int ret = httpd_req_recv(req, buf, 255);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    delete[] buf;
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return ESP_OK;
    }

    // 필드 추출
    cJSON* deviceId_json = cJSON_GetObjectItem(root, "deviceId");

    // deviceId 배열 파싱
    uint8_t device_id[2] = {0xFF, 0xFF};  // 기본 broadcast
    if (deviceId_json && cJSON_IsArray(deviceId_json) && cJSON_GetArraySize(deviceId_json) >= 2) {
        device_id[0] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 0)->valueint;
        device_id[1] = (uint8_t)cJSON_GetArrayItem(deviceId_json, 1)->valueint;
    }

    cJSON_Delete(root);

    // REBOOT 요청 이벤트 발행
    event_bus_publish(EVT_DEVICE_REBOOT_REQUEST, device_id, sizeof(device_id));

    T_LOGD(TAG, "Device reboot request: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t api_status_request_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 상태 요청 이벤트 발행
    event_bus_publish(EVT_STATUS_REQUEST, nullptr, 0);

    T_LOGD(TAG, "Status request sent (Broadcast)");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

#endif // DEVICE_MODE_TX

} // extern "C"
