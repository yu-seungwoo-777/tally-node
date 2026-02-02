/**
 * @file api_lora.cpp
 * @brief API LoRa 핸들러 구현
 */

#include "api_lora.h"
#include "web_server_helpers.h"
#include "web_server_cache.h"
#include "event_bus.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "02_WS_LoRa";

extern "C" {

esp_err_t api_lora_scan_get_handler(httpd_req_t* req)
{
    T_LOGD(TAG, "GET /api/lora/scan");
    web_server_set_cors_headers(req);

    const web_server_data_t* cache = web_server_cache_get();

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return web_server_send_json_internal_error(req, "Memory allocation failed");
    }

    cJSON_AddBoolToObject(root, "scanning", web_server_cache_is_lora_scanning());
    cJSON_AddNumberToObject(root, "progress", web_server_cache_get_lora_scan_progress());

    // 스캔 결과
    cJSON* results = cJSON_CreateArray();
    if (web_server_cache_is_lora_scan_valid()) {
        for (uint8_t i = 0; i < cache->lora_scan.count; i++) {
            cJSON* channel = cJSON_CreateObject();
            cJSON_AddNumberToObject(channel, "frequency", cache->lora_scan.channels[i].frequency);
            cJSON_AddNumberToObject(channel, "rssi", cache->lora_scan.channels[i].rssi);
            cJSON_AddNumberToObject(channel, "noiseFloor", cache->lora_scan.channels[i].noise_floor);
            cJSON_AddBoolToObject(channel, "clearChannel", cache->lora_scan.channels[i].clear_channel);
            cJSON_AddStringToObject(channel, "status", cache->lora_scan.channels[i].clear_channel ? "clear" : "busy");

            cJSON_AddItemToArray(results, channel);
        }
    }
    cJSON_AddItemToObject(root, "results", results);

    return web_server_send_json_response(req, root);
}

esp_err_t api_lora_scan_start_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 크기 사전 검증
    if (!web_server_validate_content_length(req, 256)) {
        return ESP_FAIL;
    }

    // 요청 바디 읽기 (스택 할당)
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return web_server_send_json_bad_request(req, "Failed to read body");
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (root == nullptr) {
        return web_server_send_json_bad_request(req, "Invalid JSON");
    }

    // 칩 타입 확인 후 기본 범위 설정 (캐시된 시스템 정보 사용)
    const web_server_data_t* cache = web_server_cache_get();
    uint8_t chip_type = cache->system.lora_chip_type;  // 0=Unknown, 1=SX1262_868M, 2=SX1268_433M

    float default_start, default_end;
    bool is_433_module = (chip_type == 2);  // SX1268_433M

    if (is_433_module) {
        default_start = 410.0f;  // 433MHz 모듈 기본 범위
        default_end = 493.0f;
    } else {
        default_start = 850.0f;  // 868MHz 모듈 기본 범위
        default_end = 930.0f;
    }

    // 파라미터 추출 (기본값: 칩 타입에 따른 범위)
    cJSON* start_json = cJSON_GetObjectItem(root, "startFreq");
    cJSON* end_json = cJSON_GetObjectItem(root, "endFreq");

    float start_freq = default_start;
    float end_freq = default_end;
    float step = 1.0f;

    if (start_json && cJSON_IsNumber(start_json)) {
        start_freq = (float)start_json->valuedouble;
        // 범위 검증
        if (is_433_module) {
            if (start_freq < 410.0f || start_freq > 493.0f) {
                cJSON_Delete(root);
                return web_server_send_json_error(req, "Start frequency out of range (410-493 MHz for 433MHz module)");
            }
        } else {
            if (start_freq < 850.0f || start_freq > 930.0f) {
                cJSON_Delete(root);
                return web_server_send_json_error(req, "Start frequency out of range (850-930 MHz for 868MHz module)");
            }
        }
    }
    if (end_json && cJSON_IsNumber(end_json)) {
        end_freq = (float)end_json->valuedouble;
        // 범위 검증
        if (is_433_module) {
            if (end_freq < 410.0f || end_freq > 493.0f) {
                cJSON_Delete(root);
                return web_server_send_json_error(req, "End frequency out of range (410-493 MHz for 433MHz module)");
            }
        } else {
            if (end_freq < 850.0f || end_freq > 930.0f) {
                cJSON_Delete(root);
                return web_server_send_json_error(req, "End frequency out of range (850-930 MHz for 868MHz module)");
            }
        }
    }

    cJSON_Delete(root);

    // 스캔 시작 이벤트 발행
    lora_scan_start_t scan_req = {
        .start_freq = start_freq,
        .end_freq = end_freq,
        .step = step
    };
    event_bus_publish(EVT_LORA_SCAN_START, &scan_req, sizeof(scan_req));

    return web_server_send_json_ok(req);
}

esp_err_t api_lora_scan_stop_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 스캔 중지 이벤트 발행
    event_bus_publish(EVT_LORA_SCAN_STOP, nullptr, 0);

    web_server_cache_set_lora_scan_stopped();

    return web_server_send_json_ok(req);
}

} // extern "C"
