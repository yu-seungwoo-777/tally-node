/**
 * @file api_lora.cpp
 * @brief API LoRa 핸들러 구현
 */

#include "api_lora.h"
#include "web_server_cache.h"
#include "event_bus.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "02_WebSvr_LoRa";

static void set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

extern "C" {

esp_err_t api_lora_scan_get_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    const web_server_data_t* cache = web_server_cache_get();

    cJSON* root = cJSON_CreateObject();

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

    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_lora_scan_start_handler(httpd_req_t* req)
{
    set_cors_headers(req);

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

    if (root == nullptr) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 파라미터 추출 (기본값: 863-870 MHz, 0.1 MHz step)
    cJSON* start_json = cJSON_GetObjectItem(root, "startFreq");
    cJSON* end_json = cJSON_GetObjectItem(root, "endFreq");
    cJSON* step_json = cJSON_GetObjectItem(root, "step");

    float start_freq = 863.0f;
    float end_freq = 870.0f;
    float step = 0.1f;

    if (start_json && cJSON_IsNumber(start_json)) {
        start_freq = (float)start_json->valuedouble;
    }
    if (end_json && cJSON_IsNumber(end_json)) {
        end_freq = (float)end_json->valuedouble;
    }
    if (step_json && cJSON_IsNumber(step_json)) {
        step = (float)step_json->valuedouble;
    }

    cJSON_Delete(root);

    // 스캔 시작 이벤트 발행
    lora_scan_start_t scan_req = {
        .start_freq = start_freq,
        .end_freq = end_freq,
        .step = step
    };
    event_bus_publish(EVT_LORA_SCAN_START, &scan_req, sizeof(scan_req));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"started\"}");

    return ESP_OK;
}

esp_err_t api_lora_scan_stop_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // 스캔 중지 이벤트 발행
    event_bus_publish(EVT_LORA_SCAN_STOP, nullptr, 0);

    web_server_cache_set_lora_scan_stopped();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"stopped\"}");

    return ESP_OK;
}

} // extern "C"
