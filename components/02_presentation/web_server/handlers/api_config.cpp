/**
 * @file api_config.cpp
 * @brief API Config 핸들러 구현
 */

#include "api_config.h"
#include "web_server_config.h"
#include "web_server_helpers.h"
#include "web_server_cache.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include <cstring>

static const char* TAG = "02_WebSvr_Config";
static const char* TAG_RF = "02_RF";

extern "C" {

esp_err_t api_config_post_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    const char* uri = req->uri;
    const char* prefix = "/api/config/";
    size_t prefix_len = strlen(prefix);

    if (strncmp(uri, prefix, prefix_len) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI");
        return ESP_FAIL;
    }

    const char* path = uri + prefix_len;

    // 요청 바디 읽기
    char* buf = new char[512];
    int ret = httpd_req_recv(req, buf, 511);
    if (ret <= 0) {
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (root == nullptr) {
        T_LOGE(TAG, "POST /api/config/%s JSON parse failed", path);
        delete[] buf;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    delete[] buf;

    // 설정 저장 요청 이벤트 데이터
    config_save_request_t save_req = {};
    esp_err_t parse_result = ESP_OK;

    // 경로별 처리
    if (strncmp(path, "device/rf", 9) == 0) {
        cJSON* freq = cJSON_GetObjectItem(root, "frequency");
        cJSON* sync = cJSON_GetObjectItem(root, "syncWord");
        if (freq && sync && cJSON_IsNumber(freq) && cJSON_IsNumber(sync)) {
            // RF 설정은 즉시 적용 (broadcast 후 NVS 저장)
            lora_rf_event_t rf_event;
            rf_event.frequency = (float)freq->valuedouble;
            rf_event.sync_word = (uint8_t)sync->valueint;
            event_bus_publish(EVT_RF_CHANGED, &rf_event, sizeof(rf_event));

            T_LOGD(TAG_RF, "RF config request: %.1f MHz, Sync 0x%02X",
                     rf_event.frequency, rf_event.sync_word);

            cJSON_Delete(root);

            // 응답 (NVS 저장은 broadcast 완료 후 처리됨)
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
            return ESP_OK;
        } else {
            T_LOGE(TAG, "Missing 'frequency' or 'syncWord'");
            cJSON_Delete(root);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                               "Missing 'frequency' or 'syncWord'");
            return ESP_FAIL;
        }
    }
    else if (strncmp(path, "switcher/primary", 16) == 0) {
        parse_result = web_server_config_parse_switcher_primary(root, &save_req);
    }
    else if (strncmp(path, "switcher/secondary", 18) == 0) {
        parse_result = web_server_config_parse_switcher_secondary(root, &save_req);
    }
    else if (strncmp(path, "switcher/dual", 13) == 0) {
        parse_result = web_server_config_parse_switcher_dual(root, &save_req);
    }
    else if (strncmp(path, "network/ap", 11) == 0) {
        parse_result = web_server_config_parse_network_ap(root, &save_req);
    }
    else if (strncmp(path, "network/wifi", 13) == 0) {
        parse_result = web_server_config_parse_network_wifi(root, &save_req);
    }
    else if (strncmp(path, "network/ethernet", 16) == 0) {
        parse_result = web_server_config_parse_network_ethernet(root, &save_req);
    }
    else {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown config path");
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    if (parse_result != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                           "Failed to parse config");
        return ESP_FAIL;
    }

    // 설정 저장 이벤트 발행
    event_bus_publish(EVT_CONFIG_CHANGED, &save_req, sizeof(save_req));

    // EVT_CONFIG_DATA_CHANGED 이벤트가 network_service에 전달될 때까지 대기
    vTaskDelay(pdMS_TO_TICKS(100));

    // 네트워크 설정인 경우 재시작 이벤트도 발행
    web_server_config_publish_network_restart(&save_req);

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

} // extern "C"
