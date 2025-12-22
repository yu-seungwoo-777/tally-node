/**
 * @file ApiHandler.cpp
 * @brief API 핸들러 구현 (TX 전용)
 */

// TX 모드에서만 빌드
#ifdef DEVICE_MODE_TX

#include "log.h"
#include "log_tags.h"
#include "ApiHandler.h"
#include "ConfigCore.h"
#include "NetworkManager.h"
#include "WiFiCore.h"
#include "SwitcherManager.h"
// #include "../../display/include/DisplayManager.h"  // 디스플레이 제거됨
#include "service/TallyDispatcher.h"
#include "LoRaManager.h"
#include "switcher.h"
#include "atem_protocol.h"
#include "vmix_protocol.h"
#include "obs_protocol.h"
#include "log.h"
#include "log_tags.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = TAG_API;

// ============================================================================
// API 핸들러
// ============================================================================

esp_err_t ApiHandler::configGetHandler(httpd_req_t* req)
{
    ConfigWiFiSTA wifi_sta = ConfigCore::getWiFiSTA();
    ConfigWiFiAP wifi_ap = ConfigCore::getWiFiAP();
    ConfigEthernet eth = ConfigCore::getEthernet();

    cJSON* root = cJSON_CreateObject();

    // WiFi STA
    cJSON* wifi_sta_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_sta_obj, "ssid", wifi_sta.ssid);
    cJSON_AddStringToObject(wifi_sta_obj, "password", wifi_sta.password);
    cJSON_AddItemToObject(root, "wifi_sta", wifi_sta_obj);

    // WiFi AP
    cJSON* wifi_ap_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_ap_obj, "ssid", wifi_ap.ssid);
    cJSON_AddStringToObject(wifi_ap_obj, "password", wifi_ap.password);
    cJSON_AddItemToObject(root, "wifi_ap", wifi_ap_obj);

    // Ethernet
    cJSON* eth_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth_obj, "dhcp_enabled", eth.dhcp_enabled);
    cJSON_AddStringToObject(eth_obj, "static_ip", eth.static_ip);
    cJSON_AddStringToObject(eth_obj, "static_netmask", eth.static_netmask);
    cJSON_AddStringToObject(eth_obj, "static_gateway", eth.static_gateway);
    cJSON_AddItemToObject(root, "eth", eth_obj);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t ApiHandler::wifiScanHandler(httpd_req_t* req)
{
    const uint16_t MAX_APS = 10;  // 스택 오버플로우 방지를 위해 10개로 제한

    // 동적 메모리 할당 (스택 부담 감소)
    WiFiScanResult* scan_results = (WiFiScanResult*)malloc(sizeof(WiFiScanResult) * MAX_APS);
    if (!scan_results) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "메모리 부족");
        return ESP_FAIL;
    }

    uint16_t ap_count = 0;

    // WiFi 스캔 실행
    esp_err_t err = WiFiCore::scan(scan_results, MAX_APS, &ap_count);
    if (err != ESP_OK) {
        free(scan_results);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi 스캔 실패");
        return ESP_FAIL;
    }

    // JSON 응답 생성
    cJSON* root = cJSON_CreateObject();
    cJSON* networks = cJSON_CreateArray();

    for (int i = 0; i < ap_count; i++) {
        cJSON* network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", scan_results[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", scan_results[i].rssi);
        cJSON_AddNumberToObject(network, "channel", scan_results[i].channel);

        // auth_mode를 문자열로 변환
        const char* auth_str = "Open";
        switch (scan_results[i].auth_mode) {
            case WIFI_AUTH_WEP: auth_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK: auth_str = "WPA2/WPA3"; break;
            default: auth_str = "Unknown"; break;
        }
        cJSON_AddStringToObject(network, "auth", auth_str);

        cJSON_AddItemToArray(networks, network);
    }

    cJSON_AddItemToObject(root, "networks", networks);
    cJSON_AddNumberToObject(root, "count", ap_count);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    free(scan_results);  // 동적 메모리 해제

    return ESP_OK;
}

esp_err_t ApiHandler::configWifiHandler(httpd_req_t* req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "잘못된 요청");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 파싱 실패");
        return ESP_FAIL;
    }

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    const char* ssid_str = (ssid && cJSON_IsString(ssid)) ? ssid->valuestring : "";
    const char* password_str = (password && cJSON_IsString(password)) ? password->valuestring : "";

    // ConfigCore로 설정 저장
    ConfigWiFiSTA config;
    strncpy(config.ssid, ssid_str, sizeof(config.ssid) - 1);
    strncpy(config.password, password_str, sizeof(config.password) - 1);

    esp_err_t err = ConfigCore::setWiFiSTA(config);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "설정 저장 실패");
        return ESP_FAIL;
    }

    // WiFi 재시작
    NetworkManager::restartWiFi();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::configEthHandler(httpd_req_t* req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "잘못된 요청");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 파싱 실패");
        return ESP_FAIL;
    }

    cJSON* dhcp = cJSON_GetObjectItem(root, "dhcp_enabled");
    cJSON* ip = cJSON_GetObjectItem(root, "static_ip");
    cJSON* netmask = cJSON_GetObjectItem(root, "static_netmask");
    cJSON* gateway = cJSON_GetObjectItem(root, "static_gateway");

    bool dhcp_enabled = (dhcp && cJSON_IsBool(dhcp)) ? cJSON_IsTrue(dhcp) : true;
    const char* ip_str = (ip && cJSON_IsString(ip)) ? ip->valuestring : "";
    const char* netmask_str = (netmask && cJSON_IsString(netmask)) ? netmask->valuestring : "";
    const char* gateway_str = (gateway && cJSON_IsString(gateway)) ? gateway->valuestring : "";

    // ConfigCore로 설정 저장
    ConfigEthernet config;
    config.dhcp_enabled = dhcp_enabled;
    if (!dhcp_enabled) {
        strncpy(config.static_ip, ip_str, sizeof(config.static_ip) - 1);
        strncpy(config.static_netmask, netmask_str, sizeof(config.static_netmask) - 1);
        strncpy(config.static_gateway, gateway_str, sizeof(config.static_gateway) - 1);
    }

    esp_err_t err = ConfigCore::setEthernet(config);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "설정 저장 실패");
        return ESP_FAIL;
    }

    // Ethernet 재시작
    NetworkManager::restartEthernet();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::configSwitchersGetHandler(httpd_req_t* req)
{
    cJSON* root = cJSON_CreateObject();

    // 듀얼 모드 플래그 추가
    bool dual_mode = ConfigCore::getDualMode();
    cJSON_AddBoolToObject(root, "dual_mode", dual_mode);

    cJSON* switchers = cJSON_CreateArray();

    // Primary와 Secondary 스위처 설정 가져오기
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        switcher_index_t index = (switcher_index_t)i;
        ConfigSwitcher sw = ConfigCore::getSwitcher(index);

        cJSON* sw_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(sw_obj, "index", i);
        // enabled는 dual_mode와 index로 판단
        bool is_enabled = (i == SWITCHER_INDEX_PRIMARY) ||
                         (i == SWITCHER_INDEX_SECONDARY && SwitcherManager::isDualMode());
        cJSON_AddBoolToObject(sw_obj, "enabled", is_enabled);
        cJSON_AddNumberToObject(sw_obj, "type", sw.type);
        cJSON_AddNumberToObject(sw_obj, "interface", sw.interface);
        cJSON_AddStringToObject(sw_obj, "ip", sw.ip);
        cJSON_AddNumberToObject(sw_obj, "port", sw.port);
        cJSON_AddStringToObject(sw_obj, "password", sw.password);

        // 런타임 상태 추가
        bool is_connected = SwitcherManager::isConnected(index);
        cJSON_AddBoolToObject(sw_obj, "connected", is_connected);

        switcher_t* handle = SwitcherManager::getHandle(index);
        if (handle) {
            // 스위처 정보 (product, num_cameras) - 연결된 경우만
            if (is_connected) {
                switcher_info_t info;
                if (switcher_get_info(handle, &info) == SWITCHER_OK) {
                    cJSON_AddStringToObject(sw_obj, "product", info.product_name);
                    cJSON_AddNumberToObject(sw_obj, "num_cameras", info.num_cameras);
                } else {
                    cJSON_AddStringToObject(sw_obj, "product", "");
                    cJSON_AddNumberToObject(sw_obj, "num_cameras", 0);
                }
            } else {
                cJSON_AddStringToObject(sw_obj, "product", "");
                cJSON_AddNumberToObject(sw_obj, "num_cameras", 0);
            }

            // 매핑 정보
            uint8_t camera_limit = switcher_get_camera_limit(handle);
            uint8_t camera_offset = switcher_get_camera_offset(handle);
            // effective_count는 연결된 경우만 유효한 값
            uint8_t effective_count = is_connected ? switcher_get_effective_camera_count(handle) : 0;

            cJSON_AddNumberToObject(sw_obj, "camera_limit", camera_limit);
            cJSON_AddNumberToObject(sw_obj, "camera_offset", camera_offset);
            cJSON_AddNumberToObject(sw_obj, "effective_count", effective_count);
        } else {
            cJSON_AddStringToObject(sw_obj, "product", "");
            cJSON_AddNumberToObject(sw_obj, "num_cameras", 0);
            cJSON_AddNumberToObject(sw_obj, "camera_limit", 0);
            cJSON_AddNumberToObject(sw_obj, "camera_offset", 0);
            cJSON_AddNumberToObject(sw_obj, "effective_count", 0);
        }

        cJSON_AddItemToArray(switchers, sw_obj);
    }

    cJSON_AddItemToObject(root, "switchers", switchers);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t ApiHandler::configSwitcherSetHandler(httpd_req_t* req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "잘못된 요청");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 파싱 실패");
        return ESP_FAIL;
    }

    // index 가져오기
    cJSON* index_json = cJSON_GetObjectItem(root, "index");
    if (!index_json || !cJSON_IsNumber(index_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "index 필드 누락");
        return ESP_FAIL;
    }

    int index = index_json->valueint;
    if (index < 0 || index >= SWITCHER_INDEX_MAX) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "잘못된 index 값");
        return ESP_FAIL;
    }

    // 기존 설정 로드 (camera_offset, camera_limit 유지)
    ConfigSwitcher config = ConfigCore::getSwitcher((switcher_index_t)index);

    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* interface = cJSON_GetObjectItem(root, "interface");
    cJSON* ip = cJSON_GetObjectItem(root, "ip");
    cJSON* password = cJSON_GetObjectItem(root, "password");
    cJSON* camera_limit = cJSON_GetObjectItem(root, "camera_limit");

    config.type = (type && cJSON_IsNumber(type)) ? (switcher_type_t)type->valueint : SWITCHER_TYPE_UNKNOWN;
    config.interface = (interface && cJSON_IsNumber(interface)) ? (switcher_interface_t)interface->valueint : SWITCHER_INTERFACE_NONE;

    // 포트는 타입에 따라 자동 설정
    switch (config.type) {
        case SWITCHER_TYPE_ATEM:
            config.port = ATEM_DEFAULT_PORT;
            break;
        case SWITCHER_TYPE_VMIX:
            config.port = VMIX_DEFAULT_PORT;
            break;
        case SWITCHER_TYPE_OBS:
            config.port = OBS_DEFAULT_PORT;
            break;
        default:
            config.port = 0;
            break;
    }

    if (ip && cJSON_IsString(ip)) {
        strncpy(config.ip, ip->valuestring, sizeof(config.ip) - 1);
    }

    if (password && cJSON_IsString(password)) {
        strncpy(config.password, password->valuestring, sizeof(config.password) - 1);
    }

    if (camera_limit && cJSON_IsNumber(camera_limit)) {
        config.camera_limit = (uint8_t)camera_limit->valueint;
    }

    // 설정 저장
    esp_err_t err = ConfigCore::setSwitcher((switcher_index_t)index, config);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "설정 저장 실패");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::configSwitcherMappingHandler(httpd_req_t* req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 파라미터 파싱
    cJSON* index_obj = cJSON_GetObjectItem(root, "index");
    cJSON* limit_obj = cJSON_GetObjectItem(root, "camera_limit");
    cJSON* offset_obj = cJSON_GetObjectItem(root, "camera_offset");

    if (!index_obj || !cJSON_IsNumber(index_obj)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid index");
        return ESP_FAIL;
    }

    switcher_index_t index = (switcher_index_t)index_obj->valueint;

    // SwitcherManager를 통해 스위처 핸들 가져오기
    switcher_t* sw = SwitcherManager::getHandle(index);
    if (!sw) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Switcher not found");
        return ESP_FAIL;
    }

    // 현재 설정 불러오기
    ConfigSwitcher config = ConfigCore::getSwitcher(index);

    const char* sw_name = (index == SWITCHER_INDEX_PRIMARY) ? "Primary" : "Secondary";

    // camera_limit 설정 (메모리 + NVS)
    if (limit_obj && cJSON_IsNumber(limit_obj)) {
        uint8_t limit = (uint8_t)limit_obj->valueint;
        switcher_set_camera_limit(sw, limit);
        config.camera_limit = limit;
        LOG_1(TAG, "스위처 %s 카메라 제한: %d", sw_name, limit);
    }

    // camera_offset 설정 (메모리 + NVS)
    if (offset_obj && cJSON_IsNumber(offset_obj)) {
        uint8_t offset = (uint8_t)offset_obj->valueint;
        switcher_set_camera_offset(sw, offset);
        config.camera_offset = offset;
        LOG_1(TAG, "스위처 %s 카메라 오프셋: %d", sw_name, offset);
    }

    // NVS에 저장
    esp_err_t err = ConfigCore::setSwitcher(index, config);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save mapping");
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    // 웹 설정 변경 구분선
    LOG_0(TAG, "");
    LOG_0(TAG, "----- 웹 UI: 맵핑 설정 변경 -----");

#ifdef DEVICE_MODE_TX
    // FastTallyMapper 재초기화 (새로운 offset 적용)
    TallyDispatcher::reinitializeMapper();

    // 매핑 정보 로그 출력 (TX 전용)
    TallyDispatcher::logMappingInfo();

    // 매핑 변경 강제 업데이트 (디스플레이 + LoRa 송신) (TX 전용)
    TallyDispatcher::forceUpdate();
#endif

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::restartHandler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"restarting\"}");

    // 시스템 재시작
    LOG_0(TAG, "시스템 재시작 요청... 3초 후 재시작");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

    return ESP_OK;
}

esp_err_t ApiHandler::switcherRestartHandler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    // 스위처 재시작
    LOG_0(TAG, "스위처 연결 재시작 요청");
    SwitcherManager::restartAll();

    return ESP_OK;
}

esp_err_t ApiHandler::configModeHandler(httpd_req_t* req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "요청 읽기 실패");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 파싱 실패");
        return ESP_FAIL;
    }

    cJSON* dual_mode_json = cJSON_GetObjectItem(root, "dual_mode");
    if (!dual_mode_json || !cJSON_IsBool(dual_mode_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "dual_mode 필드 누락 또는 잘못된 타입");
        return ESP_FAIL;
    }

    bool dual_mode = cJSON_IsTrue(dual_mode_json);
    cJSON_Delete(root);

    // ConfigCore에 저장
    esp_err_t err = ConfigCore::setDualMode(dual_mode);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "모드 설정 저장 실패");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::loraScanHandler(httpd_req_t* req)
{
    // 쿼리 파라미터 파싱
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "쿼리 파라미터 누락");
        return ESP_FAIL;
    }

    char start_str[16] = {0};
    char end_str[16] = {0};

    // 파라미터 추출
    httpd_query_key_value(query, "start", start_str, sizeof(start_str));
    httpd_query_key_value(query, "end", end_str, sizeof(end_str));

    if (start_str[0] == 0 || end_str[0] == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "start, end 파라미터 필요");
        return ESP_FAIL;
    }

    float start_freq = atof(start_str);
    float end_freq = atof(end_str);
    float step = 1.0f;  // 고정값: 1MHz

    // 최대 100개 채널 스캔
    const size_t MAX_CHANNELS = 100;
    channel_info_t* results = (channel_info_t*)malloc(sizeof(channel_info_t) * MAX_CHANNELS);
    if (!results) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "메모리 부족");
        return ESP_FAIL;
    }

    size_t result_count = 0;

    // 스캔 실행
    esp_err_t err = LoRaManager::scanChannels(start_freq, end_freq, step, results, MAX_CHANNELS, &result_count);
    if (err != ESP_OK) {
        free(results);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "스캔 실패");
        return ESP_FAIL;
    }

    // JSON 응답 생성
    cJSON* root = cJSON_CreateObject();
    cJSON* channels = cJSON_CreateArray();

    for (size_t i = 0; i < result_count; i++) {
        cJSON* channel = cJSON_CreateObject();
        cJSON_AddNumberToObject(channel, "frequency", results[i].frequency);
        cJSON_AddNumberToObject(channel, "rssi", results[i].rssi);
        cJSON_AddNumberToObject(channel, "noise_floor", results[i].noise_floor);
        cJSON_AddBoolToObject(channel, "clear_channel", results[i].clear_channel);
        cJSON_AddItemToArray(channels, channel);
    }

    cJSON_AddItemToObject(root, "channels", channels);
    cJSON_AddNumberToObject(root, "count", result_count);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    free(results);

    return ESP_OK;
}

esp_err_t ApiHandler::loraConfigHandler(httpd_req_t* req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "요청 읽기 실패");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON 파싱 실패");
        return ESP_FAIL;
    }

    cJSON* freq_json = cJSON_GetObjectItem(root, "frequency");
    cJSON* sync_json = cJSON_GetObjectItem(root, "sync_word");

    // 주파수 임시 저장
    if (freq_json && cJSON_IsNumber(freq_json)) {
        float frequency = (float)freq_json->valuedouble;
        LoRaManager::setPendingFrequency(frequency);
    }

    // Sync Word 임시 저장
    if (sync_json && cJSON_IsNumber(sync_json)) {
        uint8_t sync_word = (uint8_t)sync_json->valueint;
        LoRaManager::setPendingSyncWord(sync_word);
    }

    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

esp_err_t ApiHandler::loraStatusHandler(httpd_req_t* req)
{
    lora_status_t status = LoRaManager::getStatus();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "initialized", status.is_initialized);
    cJSON_AddNumberToObject(root, "chip_type", (int)status.chip_type);
    cJSON_AddStringToObject(root, "chip_name", LoRaCore::getChipName());
    cJSON_AddNumberToObject(root, "frequency", status.frequency);
    cJSON_AddNumberToObject(root, "rssi", status.rssi);
    cJSON_AddNumberToObject(root, "snr", status.snr);
    cJSON_AddNumberToObject(root, "freq_min", status.freq_min);
    cJSON_AddNumberToObject(root, "freq_max", status.freq_max);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);

    return ESP_OK;
}

esp_err_t ApiHandler::loraApplyHandler(httpd_req_t* req)
{
    // 임시 저장된 설정 확인
    if (!LoRaManager::hasPendingConfig()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"no_pending\"}");
        return ESP_OK;
    }

    // 설정 적용 (3회 송신 + 1초 대기 + TX 변경)
    esp_err_t err = LoRaManager::applyPendingConfig();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "설정 적용 실패");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

#endif  // DEVICE_MODE_TX
