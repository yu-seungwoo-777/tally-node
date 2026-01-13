/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API (Event-based)
 */

#include "web_server.h"
#include "license_service.h"
#include "event_bus.h"
#include "lora_protocol.h"
#include "app_types.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "t_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <cstring>
#include <cmath>
#include <sys/socket.h>
#include <netdb.h>  // gethostbyname

static const char* TAG = "02_WebSvr";
static const char* TAG_RF = "02_RF";
static httpd_handle_t s_server = nullptr;

// ============================================================================
// 정적 파일 (임베디드) - web/ 폴더에서 build 시 생성됨
// ============================================================================
#include "static_files.h"

// ============================================================================
// 내부 데이터 캐시 (event_bus 구조체 그대로 사용)
// ============================================================================

typedef struct {
    system_info_event_t system;       // EVT_INFO_UPDATED
    bool system_valid;

    switcher_status_event_t switcher; // EVT_SWITCHER_STATUS_CHANGED
    bool switcher_valid;

    network_status_event_t network;   // EVT_NETWORK_STATUS_CHANGED
    bool network_valid;

    config_data_event_t config;       // EVT_CONFIG_DATA_CHANGED
    bool config_valid;

    // LoRa 스캔 결과
    lora_scan_complete_t lora_scan;   // EVT_LORA_SCAN_COMPLETE
    bool lora_scan_valid;
    bool lora_scanning;               // 스캔 중 여부
    uint8_t lora_scan_progress;       // 스캔 진행률

    // 디바이스 리스트 (TX 전용)
    device_list_event_t devices;      // EVT_DEVICE_LIST_CHANGED
    bool devices_valid;

    // 라이센스 상태
    license_state_event_t license;    // EVT_LICENSE_STATE_CHANGED
    bool license_valid;
} web_server_data_t;

static web_server_data_t s_cache;
static SemaphoreHandle_t s_cache_mutex = nullptr;  // s_cache 보호용 뮤텍스

/**
 * @brief 내부 데이터 캐시 초기화
 * @details 캐시 구조체를 0으로 초기화하고 뮤텍스를 생성합니다
 */
static void init_cache(void)
{
    memset(&s_cache, 0, sizeof(s_cache));
    s_cache.system_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.config_valid = false;
    s_cache.lora_scan_valid = false;
    s_cache.lora_scanning = false;
    s_cache.lora_scan_progress = 0;
    s_cache.devices_valid = false;
    s_cache.license_valid = false;

    // 뮤텍스 생성 (최초 1회)
    if (s_cache_mutex == nullptr) {
        s_cache_mutex = xSemaphoreCreateMutex();
    }
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 시스템 정보 이벤트 핸들러 (EVT_INFO_UPDATED)
 */
static esp_err_t onSystemInfoEvent(const event_data_t* event)
{
    // 뮤텍스가 생성되지 않았으면 아직 초기화 안된 상태
    if (!event || !s_cache_mutex) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(system_info_event_t)) {
        T_LOGE(TAG, "System info: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(system_info_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const system_info_event_t* info = (const system_info_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&s_cache.system, info, sizeof(system_info_event_t));
        s_cache.system_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief 스위처 상태 이벤트 핸들러 (EVT_SWITCHER_STATUS_CHANGED)
 */
static esp_err_t onSwitcherStatusEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(switcher_status_event_t)) {
        T_LOGE(TAG, "Switcher status: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(switcher_status_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const switcher_status_event_t* status = (const switcher_status_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&s_cache.switcher, status, sizeof(switcher_status_event_t));
        s_cache.switcher_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief 네트워크 상태 이벤트 핸들러 (EVT_NETWORK_STATUS_CHANGED)
 */
static esp_err_t onNetworkStatusEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(network_status_event_t)) {
        T_LOGE(TAG, "Network status: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(network_status_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const network_status_event_t* status = (const network_status_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&s_cache.network, status, sizeof(network_status_event_t));
        s_cache.network_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief 설정 데이터 이벤트 핸들러 (EVT_CONFIG_DATA_CHANGED)
 */
static esp_err_t onConfigDataEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(config_data_event_t)) {
        T_LOGE(TAG, "Config data: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(config_data_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const config_data_event_t* config = (const config_data_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_cache.config = *config;
        s_cache.config_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief LoRa 스캔 시작 이벤트 핸들러 (EVT_LORA_SCAN_START)
 */
static esp_err_t onLoraScanStartEvent(const event_data_t* event)
{
    (void)event;

    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_cache.lora_scanning = true;
        s_cache.lora_scan_progress = 0;
        s_cache.lora_scan_valid = false;
        s_cache.lora_scan.count = 0;  // 이전 결과 초기화
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief LoRa 스캔 진행 이벤트 핸들러 (EVT_LORA_SCAN_PROGRESS)
 */
static esp_err_t onLoraScanProgressEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(lora_scan_progress_t)) {
        T_LOGE(TAG, "LoRa scan progress: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(lora_scan_progress_t));
        return ESP_ERR_INVALID_ARG;
    }

    const lora_scan_progress_t* progress = (const lora_scan_progress_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_cache.lora_scan_progress = progress->progress;

        // 진행 중인 채널 결과 추가 (누적) - 버퍼 오버플로우 방지
        if (s_cache.lora_scan.count < 100) {
            s_cache.lora_scan.channels[s_cache.lora_scan.count] = progress->result;
            s_cache.lora_scan.count++;
            s_cache.lora_scan_valid = true;
        } else {
            T_LOGW(TAG, "LoRa scan progress: channel buffer full (100), ignoring");
        }
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief LoRa 스캔 완료 이벤트 핸들러 (EVT_LORA_SCAN_COMPLETE)
 */
static esp_err_t onLoraScanCompleteEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(lora_scan_complete_t)) {
        T_LOGE(TAG, "LoRa scan complete: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(lora_scan_complete_t));
        return ESP_ERR_INVALID_ARG;
    }

    const lora_scan_complete_t* result = (const lora_scan_complete_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // count 유효성 검증 (최대 100 채널)
        if (result->count > 100) {
            T_LOGW(TAG, "LoRa scan: count=%d exceeds limit, clamping to 100", result->count);
            memcpy(&s_cache.lora_scan, result, sizeof(lora_scan_complete_t));
            s_cache.lora_scan.count = 100;
        } else {
            memcpy(&s_cache.lora_scan, result, sizeof(lora_scan_complete_t));
        }
        s_cache.lora_scan_valid = true;
        s_cache.lora_scanning = false;
        s_cache.lora_scan_progress = 100;
        xSemaphoreGive(s_cache_mutex);
    }

    return ESP_OK;
}

/**
 * @brief 디바이스 리스트 이벤트 핸들러 (EVT_DEVICE_LIST_CHANGED)
 * TX 모드 전용 - 온라인 디바이스 목록 캐시
 */
static esp_err_t onDeviceListEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(device_list_event_t)) {
        T_LOGE(TAG, "Device list: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(device_list_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const device_list_event_t* devices = (const device_list_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // count 유효성 검증 (최대 20 디바이스)
        if (devices->count > 20) {
            T_LOGW(TAG, "Device list: count=%d exceeds limit, clamping to 20", devices->count);
            memcpy(&s_cache.devices, devices, sizeof(device_list_event_t));
            s_cache.devices.count = 20;
        } else {
            memcpy(&s_cache.devices, devices, sizeof(device_list_event_t));
        }
        s_cache.devices_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    T_LOGD(TAG, "Device list updated: %d devices (registered: %d)",
             devices->count, devices->registered_count);

    return ESP_OK;
}

/**
 * @brief 라이센스 상태 이벤트 핸들러 (EVT_LICENSE_STATE_CHANGED)
 */
static esp_err_t onLicenseStateEvent(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(license_state_event_t)) {
        T_LOGE(TAG, "License state: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(license_state_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const license_state_event_t* license = (const license_state_event_t*)event->data;

    // 뮤텍스 잠금
    if (s_cache_mutex && xSemaphoreTake(s_cache_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&s_cache.license, license, sizeof(license_state_event_t));
        s_cache.license_valid = true;
        xSemaphoreGive(s_cache_mutex);
    }

    T_LOGD(TAG, "License state updated: limit=%d, state=%d",
             license->device_limit, license->state);

    return ESP_OK;
}

/**
 * @brief 네트워크 재시작 완료 이벤트 핸들러 (EVT_NETWORK_RESTARTED)
 */
static esp_err_t onNetworkRestartedEvent(const event_data_t* event)
{
    T_LOGI(TAG, "네트워크 재시작 완료 - 웹서버 재시작");

    // 웹서버가 실행 중이면 재시작
    if (s_server != nullptr) {
        httpd_stop(s_server);
        s_server = nullptr;
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 대기
    }

    // 웹서버 재시작
    return web_server_start();
}

// ============================================================================
// Packed 데이터 → PGM/PVW 리스트 변환
// ============================================================================

/**
 * @brief packed 데이터에서 채널 상태 가져오기
 * @param data packed 데이터
 * @param channel 채널 번호 (1-20)
 * @return 0=off, 1=pgm, 2=pvw, 3=both
 */
static uint8_t get_channel_state(const uint8_t* data, uint8_t channel)
{
    if (!data || channel < 1 || channel > 20) {
        return 0;
    }
    uint8_t byte_idx = (channel - 1) / 4;
    uint8_t bit_idx = ((channel - 1) % 4) * 2;
    return (data[byte_idx] >> bit_idx) & 0x03;
}

/**
 * @brief packed 데이터를 hex 문자열로 변환
 * @param data 변환할 바이트 배열
 * @param size 데이터 크기
 * @param out 출력 버퍼
 * @param out_size 출력 버퍼 크기 (최소 size * 2 + 1)
 * @details 바이트 배열을 대문자 16진수 문자열로 변환합니다 (예: {0xAB, 0xCD} → "ABCD")
 */
static void packed_to_hex(const uint8_t* data, uint8_t size, char* out, size_t out_size)
{
    if (!data || !out || out_size < (size * 2 + 1)) {
        if (out) out[0] = '\0';
        return;
    }
    for (uint8_t i = 0; i < size; i++) {
        snprintf(out + (i * 2), 3, "%02X", data[i]);
    }
}

/**
 * @brief CORS 헤더 설정
 * @param req HTTP 요청 핸들러
 * @details Cross-Origin Resource Sharing 헤더를 설정하여 웹 브라우저에서의 API 접근을 허용합니다
 */
static void set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================================
// API 핸들러 헬퍼 함수
// ============================================================================

/**
 * @brief 네트워크 AP 상태 JSON 생성
 */
static cJSON* create_network_ap_json(void)
{
    cJSON* ap = cJSON_CreateObject();
    if (!ap) {
        return nullptr;
    }

    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(ap, "enabled", s_cache.config.wifi_ap_enabled);
        cJSON_AddStringToObject(ap, "ssid", s_cache.config.wifi_ap_ssid);
        cJSON_AddStringToObject(ap, "password", s_cache.config.wifi_ap_password);
        cJSON_AddNumberToObject(ap, "channel", s_cache.config.wifi_ap_channel);
        if (s_cache.network_valid) {
            const char* ip = (s_cache.config.wifi_ap_enabled && s_cache.network.ap_ip[0] != '\0') ?
                           s_cache.network.ap_ip : "--";
            cJSON_AddStringToObject(ap, "ip", ip);
        } else {
            cJSON_AddStringToObject(ap, "ip", "--");
        }
    } else {
        cJSON_AddBoolToObject(ap, "enabled", false);
        cJSON_AddStringToObject(ap, "ssid", "--");
        cJSON_AddStringToObject(ap, "password", "");
        cJSON_AddNumberToObject(ap, "channel", 1);
        cJSON_AddStringToObject(ap, "ip", "--");
    }

    return ap;
}

/**
 * @brief 네트워크 WiFi 상태 JSON 생성
 */
static cJSON* create_network_wifi_json(void)
{
    cJSON* wifi = cJSON_CreateObject();
    if (!wifi) {
        return nullptr;
    }

    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(wifi, "enabled", s_cache.config.wifi_sta_enabled);
        cJSON_AddStringToObject(wifi, "ssid", s_cache.config.wifi_sta_ssid);
        cJSON_AddStringToObject(wifi, "password", s_cache.config.wifi_sta_password);
    } else {
        cJSON_AddBoolToObject(wifi, "enabled", false);
        cJSON_AddStringToObject(wifi, "ssid", "--");
        cJSON_AddStringToObject(wifi, "password", "");
    }

    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(wifi, "connected", s_cache.network.sta_connected);
        cJSON_AddStringToObject(wifi, "ip", s_cache.network.sta_connected ?
                               s_cache.network.sta_ip : "--");
    } else {
        cJSON_AddBoolToObject(wifi, "connected", false);
        cJSON_AddStringToObject(wifi, "ip", "--");
    }

    return wifi;
}

/**
 * @brief 네트워크 Ethernet 상태 JSON 생성
 */
static cJSON* create_network_ethernet_json(void)
{
    cJSON* ethernet = cJSON_CreateObject();
    if (!ethernet) {
        return nullptr;
    }

    if (s_cache.config_valid) {
        cJSON_AddBoolToObject(ethernet, "enabled", s_cache.config.eth_enabled);
        cJSON_AddBoolToObject(ethernet, "dhcp", s_cache.config.eth_dhcp_enabled);
        cJSON_AddStringToObject(ethernet, "staticIp", s_cache.config.eth_static_ip);
        cJSON_AddStringToObject(ethernet, "netmask", s_cache.config.eth_static_netmask);
        cJSON_AddStringToObject(ethernet, "gateway", s_cache.config.eth_static_gateway);
    } else {
        cJSON_AddBoolToObject(ethernet, "enabled", false);
        cJSON_AddBoolToObject(ethernet, "dhcp", true);
        cJSON_AddStringToObject(ethernet, "staticIp", "");
        cJSON_AddStringToObject(ethernet, "netmask", "");
        cJSON_AddStringToObject(ethernet, "gateway", "");
    }

    if (s_cache.network_valid) {
        cJSON_AddBoolToObject(ethernet, "connected", s_cache.network.eth_connected);
        cJSON_AddBoolToObject(ethernet, "detected", s_cache.network.eth_detected);
        cJSON_AddStringToObject(ethernet, "ip", s_cache.network.eth_connected ?
                               s_cache.network.eth_ip : "--");
    } else {
        cJSON_AddBoolToObject(ethernet, "connected", false);
        cJSON_AddBoolToObject(ethernet, "detected", false);
        cJSON_AddStringToObject(ethernet, "ip", "--");
    }

    return ethernet;
}

/**
 * @brief Tally 데이터 JSON 생성 (PGM/PVW 리스트 + Raw hex)
 */
static cJSON* create_tally_json(const uint8_t* tally_data, uint8_t channel_count)
{
    cJSON* tally = cJSON_CreateObject();
    if (!tally) {
        return nullptr;
    }

    cJSON* pgm = cJSON_CreateArray();
    cJSON* pvw = cJSON_CreateArray();

    for (uint8_t i = 0; i < channel_count && i < 20; i++) {
        uint8_t state = get_channel_state(tally_data, i + 1);
        if (state == 1 || state == 3) {
            cJSON_AddItemToArray(pgm, cJSON_CreateNumber(i + 1));
        }
        if (state == 2 || state == 3) {
            cJSON_AddItemToArray(pvw, cJSON_CreateNumber(i + 1));
        }
    }

    cJSON_AddItemToObject(tally, "pgm", pgm);
    cJSON_AddItemToObject(tally, "pvw", pvw);

    char hex[17] = {0};
    uint8_t bytes = (channel_count + 3) / 4;
    packed_to_hex(tally_data, bytes, hex, sizeof(hex));
    cJSON_AddStringToObject(tally, "raw", hex);
    cJSON_AddNumberToObject(tally, "channels", channel_count);

    return tally;
}

/**
 * @brief 빈 Tally 데이터 JSON 생성 (연결 안 됨)
 */
static cJSON* create_empty_tally_json(void)
{
    cJSON* tally = cJSON_CreateObject();
    if (!tally) {
        return nullptr;
    }

    cJSON_AddItemToObject(tally, "pgm", cJSON_CreateArray());
    cJSON_AddItemToObject(tally, "pvw", cJSON_CreateArray());
    cJSON_AddStringToObject(tally, "raw", "");
    cJSON_AddNumberToObject(tally, "channels", 0);

    return tally;
}

/**
 * @brief 스위처 Primary 상태 JSON 생성
 */
static cJSON* create_switcher_primary_json(void)
{
    cJSON* primary = cJSON_CreateObject();
    if (!primary) {
        return nullptr;
    }

    if (s_cache.switcher_valid) {
        cJSON_AddBoolToObject(primary, "connected", s_cache.switcher.s1_connected);
        cJSON_AddStringToObject(primary, "type", s_cache.switcher.s1_type);
        cJSON_AddStringToObject(primary, "ip", s_cache.switcher.s1_ip);
        cJSON_AddNumberToObject(primary, "port", s_cache.switcher.s1_port);

        if (s_cache.config_valid) {
            cJSON_AddNumberToObject(primary, "interface",
                                  s_cache.config.primary_interface);
            cJSON_AddNumberToObject(primary, "cameraLimit",
                                  s_cache.config.primary_camera_limit);
        } else {
            cJSON_AddNumberToObject(primary, "interface", 2);
            cJSON_AddNumberToObject(primary, "cameraLimit", 0);
        }

        cJSON* tally = create_tally_json(s_cache.switcher.s1_tally_data,
                                        s_cache.switcher.s1_channel_count);
        if (tally) {
            cJSON_AddItemToObject(primary, "tally", tally);
        }
    } else {
        cJSON_AddBoolToObject(primary, "connected", false);
        cJSON_AddStringToObject(primary, "type", "--");
        cJSON_AddStringToObject(primary, "ip", "--");
        cJSON_AddNumberToObject(primary, "port", 0);
        cJSON_AddNumberToObject(primary, "interface", 2);
        cJSON_AddNumberToObject(primary, "cameraLimit", 0);

        cJSON* tally = create_empty_tally_json();
        if (tally) {
            cJSON_AddItemToObject(primary, "tally", tally);
        }
    }

    return primary;
}

/**
 * @brief 스위처 Secondary 상태 JSON 생성
 */
static cJSON* create_switcher_secondary_json(void)
{
    cJSON* secondary = cJSON_CreateObject();
    if (!secondary) {
        return nullptr;
    }

    if (s_cache.switcher_valid) {
        cJSON_AddBoolToObject(secondary, "connected", s_cache.switcher.s2_connected);
        cJSON_AddStringToObject(secondary, "type", s_cache.switcher.s2_type);
        cJSON_AddStringToObject(secondary, "ip", s_cache.switcher.s2_ip);
        cJSON_AddNumberToObject(secondary, "port", s_cache.switcher.s2_port);

        if (s_cache.config_valid) {
            cJSON_AddNumberToObject(secondary, "interface",
                                  s_cache.config.secondary_interface);
            cJSON_AddNumberToObject(secondary, "cameraLimit",
                                  s_cache.config.secondary_camera_limit);
        } else {
            cJSON_AddNumberToObject(secondary, "interface", 1);
            cJSON_AddNumberToObject(secondary, "cameraLimit", 0);
        }

        cJSON* tally = create_tally_json(s_cache.switcher.s2_tally_data,
                                        s_cache.switcher.s2_channel_count);
        if (tally) {
            cJSON_AddItemToObject(secondary, "tally", tally);
        }
    } else {
        cJSON_AddBoolToObject(secondary, "connected", false);
        cJSON_AddStringToObject(secondary, "type", "--");
        cJSON_AddStringToObject(secondary, "ip", "--");
        cJSON_AddNumberToObject(secondary, "port", 0);
        cJSON_AddNumberToObject(secondary, "interface", 1);
        cJSON_AddNumberToObject(secondary, "cameraLimit", 0);

        cJSON* tally = create_empty_tally_json();
        if (tally) {
            cJSON_AddItemToObject(secondary, "tally", tally);
        }
    }

    return secondary;
}

/**
 * @brief 스위처 상태 JSON 생성
 */
static cJSON* create_switcher_json(void)
{
    cJSON* switcher = cJSON_CreateObject();
    if (!switcher) {
        return nullptr;
    }

    cJSON* primary = create_switcher_primary_json();
    if (primary) {
        cJSON_AddItemToObject(switcher, "primary", primary);
    }

    cJSON* secondary = create_switcher_secondary_json();
    if (secondary) {
        cJSON_AddItemToObject(switcher, "secondary", secondary);
    }

    cJSON_AddBoolToObject(switcher, "dualEnabled",
                         s_cache.switcher_valid ? s_cache.switcher.dual_mode : false);

    if (s_cache.config_valid) {
        cJSON_AddNumberToObject(switcher, "secondaryOffset",
                              s_cache.config.secondary_offset);
    } else {
        cJSON_AddNumberToObject(switcher, "secondaryOffset", 4);
    }

    return switcher;
}

/**
 * @brief 시스템 상태 JSON 생성
 */
static cJSON* create_system_json(void)
{
    cJSON* system = cJSON_CreateObject();
    if (!system) {
        return nullptr;
    }

    // 펌웨어 버전 (빌드타임에 정의된 값)
    cJSON_AddStringToObject(system, "version", FIRMWARE_VERSION);

    if (s_cache.system_valid) {
        cJSON_AddStringToObject(system, "deviceId", s_cache.system.device_id);
        cJSON_AddNumberToObject(system, "battery", s_cache.system.battery);
        cJSON_AddNumberToObject(system, "voltage",
                              round(s_cache.system.voltage * 10) / 10);
        cJSON_AddNumberToObject(system, "temperature",
                              round(s_cache.system.temperature * 10) / 10);
        cJSON_AddNumberToObject(system, "uptime", s_cache.system.uptime);
        cJSON_AddNumberToObject(system, "loraChipType",
                              s_cache.system.lora_chip_type);
    } else {
        cJSON_AddStringToObject(system, "deviceId", "0000");
        cJSON_AddNumberToObject(system, "battery", 0);
        cJSON_AddNumberToObject(system, "voltage", 0);
        cJSON_AddNumberToObject(system, "temperature", 0);
        cJSON_AddNumberToObject(system, "uptime", 0);
        cJSON_AddNumberToObject(system, "loraChipType", 0);
    }

    return system;
}

/**
 * @brief RF 설정 JSON 생성
 */
static cJSON* create_rf_json(void)
{
    cJSON* rf = cJSON_CreateObject();
    if (!rf) {
        return nullptr;
    }

    if (s_cache.config_valid) {
        cJSON_AddNumberToObject(rf, "frequency",
                              s_cache.config.device_rf_frequency);
        cJSON_AddNumberToObject(rf, "syncWord",
                              s_cache.config.device_rf_sync_word);
        cJSON_AddNumberToObject(rf, "spreadingFactor",
                              s_cache.config.device_rf_sf);
        cJSON_AddNumberToObject(rf, "codingRate", s_cache.config.device_rf_cr);
        cJSON_AddNumberToObject(rf, "bandwidth", s_cache.config.device_rf_bw);
        cJSON_AddNumberToObject(rf, "txPower", s_cache.config.device_rf_tx_power);
    } else {
        cJSON_AddNumberToObject(rf, "frequency", 868);
        cJSON_AddNumberToObject(rf, "syncWord", 0x12);
        cJSON_AddNumberToObject(rf, "spreadingFactor", 7);
        cJSON_AddNumberToObject(rf, "codingRate", 7);
        cJSON_AddNumberToObject(rf, "bandwidth", 250);
        cJSON_AddNumberToObject(rf, "txPower", 22);
    }

    return rf;
}

/**
 * @brief 브로드캐스트 설정 JSON 생성
 */
static cJSON* create_broadcast_json(void)
{
    cJSON* broadcast = cJSON_CreateObject();
    if (!broadcast) {
        return nullptr;
    }

    cJSON* rf = create_rf_json();
    if (rf) {
        cJSON_AddItemToObject(broadcast, "rf", rf);
    }

    return broadcast;
}

/**
 * @brief 라이센스 상태 JSON 생성
 */
static cJSON* create_license_json(void)
{
    cJSON* license = cJSON_CreateObject();
    if (!license) {
        return nullptr;
    }

    uint8_t device_limit = license_service_get_device_limit();
    license_state_t state = license_service_get_state();

    cJSON_AddNumberToObject(license, "deviceLimit", device_limit);
    cJSON_AddNumberToObject(license, "state", (int)state);

    const char* state_str = "unknown";
    switch (state) {
        case LICENSE_STATE_VALID:
            state_str = "valid";
            break;
        case LICENSE_STATE_INVALID:
            state_str = "invalid";
            break;
        case LICENSE_STATE_CHECKING:
            state_str = "checking";
            break;
    }
    cJSON_AddStringToObject(license, "stateStr", state_str);

    bool is_valid = (state == LICENSE_STATE_VALID);
    cJSON_AddBoolToObject(license, "isValid", is_valid);

    char key[17];
    if (license_service_get_key(key) == ESP_OK && key[0] != '\0') {
        cJSON_AddStringToObject(license, "key", key);
    } else {
        cJSON_AddStringToObject(license, "key", "");
    }

    return license;
}

// ============================================================================
// API 핸들러
// ============================================================================

/**
 * @brief Switcher 공통 설정 파싱
 * @param root JSON 루트 객체
 * @param save_req 설정 저장 요청 구조체 (결과 저장)
 */
static void parse_switcher_common_fields(cJSON* root, config_save_request_t* save_req)
{
    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* ip = cJSON_GetObjectItem(root, "ip");
    cJSON* port = cJSON_GetObjectItem(root, "port");
    cJSON* interface = cJSON_GetObjectItem(root, "interface");
    cJSON* camera_limit = cJSON_GetObjectItem(root, "cameraLimit");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    if (type && type->valuestring) {
        strncpy(save_req->switcher_type, type->valuestring,
               sizeof(save_req->switcher_type) - 1);
    }
    if (ip && cJSON_IsString(ip)) {
        strncpy(save_req->switcher_ip, ip->valuestring,
               sizeof(save_req->switcher_ip) - 1);
    }
    if (port && cJSON_IsNumber(port)) {
        save_req->switcher_port = (uint16_t)port->valueint;
    }
    if (interface && cJSON_IsNumber(interface)) {
        save_req->switcher_interface = (uint8_t)interface->valueint;
    } else {
        save_req->switcher_interface = 0; // Default: Auto
    }
    if (camera_limit && cJSON_IsNumber(camera_limit)) {
        save_req->switcher_camera_limit = (uint8_t)camera_limit->valueint;
    } else {
        save_req->switcher_camera_limit = 0; // Default: unlimited
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->switcher_password, password->valuestring,
               sizeof(save_req->switcher_password) - 1);
    } else {
        save_req->switcher_password[0] = '\0';
    }
}

/**
 * @brief 스위처 Primary 설정 파싱
 */
static esp_err_t parse_switcher_primary_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_PRIMARY;
    parse_switcher_common_fields(root, save_req);
    return ESP_OK;
}

/**
 * @brief 스위처 Secondary 설정 파싱
 */
static esp_err_t parse_switcher_secondary_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_SECONDARY;
    parse_switcher_common_fields(root, save_req);
    return ESP_OK;
}

/**
 * @brief 스위처 Dual 모드 설정 파싱
 */
static esp_err_t parse_switcher_dual_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_DUAL;

    // 파라미터 호환성: dualEnabled/enabled, secondaryOffset/offset
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
    cJSON* dual_enabled = cJSON_GetObjectItem(root, "dualEnabled");
    cJSON* offset = cJSON_GetObjectItem(root, "offset");
    cJSON* secondary_offset = cJSON_GetObjectItem(root, "secondaryOffset");

    // dualEnabled 또는 enabled 사용 (우선순위: dualEnabled > enabled)
    if (dual_enabled && cJSON_IsBool(dual_enabled)) {
        save_req->switcher_dual_enabled = cJSON_IsTrue(dual_enabled);
    } else if (enabled && cJSON_IsBool(enabled)) {
        save_req->switcher_dual_enabled = cJSON_IsTrue(enabled);
    }

    // secondaryOffset 또는 offset 사용 (우선순위: secondaryOffset > offset)
    if (secondary_offset && cJSON_IsNumber(secondary_offset)) {
        save_req->switcher_secondary_offset = (uint8_t)secondary_offset->valueint;
    } else if (offset && cJSON_IsNumber(offset)) {
        save_req->switcher_secondary_offset = (uint8_t)offset->valueint;
    }

    T_LOGD(TAG, "Publishing Dual Mode save event: enabled=%d, offset=%d",
             save_req->switcher_dual_enabled, save_req->switcher_secondary_offset);

    return ESP_OK;
}

/**
 * @brief 네트워크 AP 설정 파싱
 */
static esp_err_t parse_network_ap_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_WIFI_AP;

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");
    cJSON* channel = cJSON_GetObjectItem(root, "channel");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    if (ssid && cJSON_IsString(ssid)) {
        strncpy(save_req->wifi_ap_ssid, ssid->valuestring,
               sizeof(save_req->wifi_ap_ssid) - 1);
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->wifi_ap_password, password->valuestring,
               sizeof(save_req->wifi_ap_password) - 1);
    } else {
        save_req->wifi_ap_password[0] = '\0';  // password 없음
    }
    if (channel && cJSON_IsNumber(channel)) {
        save_req->wifi_ap_channel = (uint8_t)channel->valueint;
    }
    if (enabled && cJSON_IsBool(enabled)) {
        save_req->wifi_ap_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing AP save event: ssid=%s, pass_len=%d, ch=%d, en=%d",
             save_req->wifi_ap_ssid, strlen(save_req->wifi_ap_password),
             save_req->wifi_ap_channel, save_req->wifi_ap_enabled);

    return ESP_OK;
}

/**
 * @brief 네트워크 WiFi STA 설정 파싱
 */
static esp_err_t parse_network_wifi_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_WIFI_STA;

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    if (ssid && cJSON_IsString(ssid)) {
        strncpy(save_req->wifi_sta_ssid, ssid->valuestring,
               sizeof(save_req->wifi_sta_ssid) - 1);
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->wifi_sta_password, password->valuestring,
               sizeof(save_req->wifi_sta_password) - 1);
    } else {
        save_req->wifi_sta_password[0] = '\0';  // password 없음
    }
    if (enabled && cJSON_IsBool(enabled)) {
        save_req->wifi_sta_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing STA save event: ssid=%s, pass_len=%d, en=%d",
             save_req->wifi_sta_ssid, strlen(save_req->wifi_sta_password),
             save_req->wifi_sta_enabled);

    return ESP_OK;
}

/**
 * @brief 네트워크 Ethernet 설정 파싱
 */
static esp_err_t parse_network_ethernet_config(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_ETHERNET;

    cJSON* ip = cJSON_GetObjectItem(root, "staticIp");
    cJSON* gateway = cJSON_GetObjectItem(root, "gateway");
    cJSON* netmask = cJSON_GetObjectItem(root, "netmask");
    cJSON* dhcp = cJSON_GetObjectItem(root, "dhcp");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    if (dhcp && cJSON_IsBool(dhcp)) {
        save_req->eth_dhcp = cJSON_IsTrue(dhcp);
    }
    if (ip && cJSON_IsString(ip)) {
        strncpy(save_req->eth_static_ip, ip->valuestring,
               sizeof(save_req->eth_static_ip) - 1);
    }
    if (gateway && cJSON_IsString(gateway)) {
        strncpy(save_req->eth_gateway, gateway->valuestring,
               sizeof(save_req->eth_gateway) - 1);
    }
    if (netmask && cJSON_IsString(netmask)) {
        strncpy(save_req->eth_netmask, netmask->valuestring,
               sizeof(save_req->eth_netmask) - 1);
    }
    if (enabled && cJSON_IsBool(enabled)) {
        save_req->eth_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing Ethernet save event: dhcp=%d, en=%d",
             save_req->eth_dhcp, save_req->eth_enabled);

    return ESP_OK;
}

/**
 * @brief 네트워크 재시작 이벤트 발행
 */
static void publish_network_restart_event(const config_save_request_t* save_req)
{
    network_restart_request_t restart_req;

    memset(&restart_req, 0, sizeof(restart_req));

    if (save_req->type == CONFIG_SAVE_WIFI_AP) {
        restart_req.type = NETWORK_RESTART_WIFI_AP;
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    } else if (save_req->type == CONFIG_SAVE_WIFI_STA) {
        if (save_req->wifi_sta_enabled) {
            restart_req.type = NETWORK_RESTART_WIFI_STA;
            strncpy(restart_req.ssid, save_req->wifi_sta_ssid,
                   sizeof(restart_req.ssid) - 1);
            strncpy(restart_req.password, save_req->wifi_sta_password,
                   sizeof(restart_req.password) - 1);
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        } else {
            restart_req.type = NETWORK_RESTART_WIFI_AP;
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        }
    } else if (save_req->type == CONFIG_SAVE_ETHERNET) {
        restart_req.type = NETWORK_RESTART_ETHERNET;
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    }
}

/**
 * @brief GET /api/status - 전체 상태 반환 (캐시 데이터 사용)
 */
static esp_err_t api_status_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
    }

    // Network
    cJSON* network = cJSON_CreateObject();
    if (network) {
        cJSON* ap = create_network_ap_json();
        if (ap) {
            cJSON_AddItemToObject(network, "ap", ap);
        }

        cJSON* wifi = create_network_wifi_json();
        if (wifi) {
            cJSON_AddItemToObject(network, "wifi", wifi);
        }

        cJSON* ethernet = create_network_ethernet_json();
        if (ethernet) {
            cJSON_AddItemToObject(network, "ethernet", ethernet);
        }

        cJSON_AddItemToObject(root, "network", network);
    }

    // Switcher
    cJSON* switcher = create_switcher_json();
    if (switcher) {
        cJSON_AddItemToObject(root, "switcher", switcher);
    }

    // System
    cJSON* system = create_system_json();
    if (system) {
        cJSON_AddItemToObject(root, "system", system);
    }

    // Broadcast
    cJSON* broadcast = create_broadcast_json();
    if (broadcast) {
        cJSON_AddItemToObject(root, "broadcast", broadcast);
    }

    // License
    cJSON* license = create_license_json();
    if (license) {
        cJSON_AddItemToObject(root, "license", license);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief POST /api/config/path - 설정 저장 (이벤트 기반)
 */
static esp_err_t api_config_post_handler(httpd_req_t* req)
{
    set_cors_headers(req);

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
        T_LOGE(TAG, "POST /api/config/%s JSON 파싱 실패", path);
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

            T_LOGD(TAG_RF, "RF 설정 요청: %.1f MHz, Sync 0x%02X",
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
        parse_result = parse_switcher_primary_config(root, &save_req);
    }
    else if (strncmp(path, "switcher/secondary", 18) == 0) {
        parse_result = parse_switcher_secondary_config(root, &save_req);
    }
    else if (strncmp(path, "switcher/dual", 13) == 0) {
        parse_result = parse_switcher_dual_config(root, &save_req);
    }
    else if (strncmp(path, "network/ap", 11) == 0) {
        parse_result = parse_network_ap_config(root, &save_req);
    }
    else if (strncmp(path, "network/wifi", 13) == 0) {
        parse_result = parse_network_wifi_config(root, &save_req);
    }
    else if (strncmp(path, "network/ethernet", 16) == 0) {
        parse_result = parse_network_ethernet_config(root, &save_req);
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
    publish_network_restart_event(&save_req);

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/reboot - 시스템 재부팅
 */
static esp_err_t api_reboot_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

/**
 * @brief POST /api/reboot/broadcast - 전체 디바이스 재부팅 (브로드캐스트 + TX 재부팅)
 */
static esp_err_t api_reboot_broadcast_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    // 브로드캐스트 ID (0xFF 0xFF)
    uint8_t broadcast_id[LORA_DEVICE_ID_LEN] = {0xFF, 0xFF};

    // 이벤트 발행 (3회 송신)
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < 3; i++) {
        ret = event_bus_publish(EVT_DEVICE_REBOOT_REQUEST, broadcast_id, LORA_DEVICE_ID_LEN);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "브로드캐스트 재부팅 실패 (회차 %d): %d", i + 1, ret);
            httpd_resp_set_status(req, HTTPD_500);
            httpd_resp_sendstr(req, "{\"error\":\"Failed to send broadcast reboot\"}");
            return ESP_FAIL;
        }
    }

    if (ret == ESP_OK) {
        T_LOGI(TAG, "브로드캐스트 재부팅 명령 3회 송신 완료, 500ms 후 TX 재부팅");
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Broadcast reboot sent (3x), TX rebooting...\"}");

        // 3회 송신 후 500ms 대기 후 TX 재부팅
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    return ESP_OK;
}

/**
 * @brief POST /api/test/start - 테스트 모드 시작
 */
static esp_err_t api_test_start_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // JSON 파싱
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        T_LOGE(TAG, "httpd_req_recv failed: ret=%d", ret);
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_sendstr(req, "{\"error\":\"Invalid request\"}");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    T_LOGD(TAG, "Received JSON: %s", buf);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        T_LOGE(TAG, "cJSON_Parse failed");
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_sendstr(req, "{\"error\":\"Invalid JSON\"}");
        return ESP_FAIL;
    }

    // 파라미터 추출
    cJSON* max_channels_item = cJSON_GetObjectItem(root, "max_channels");
    cJSON* interval_ms_item = cJSON_GetObjectItem(root, "interval_ms");

    if (!max_channels_item || !interval_ms_item) {
        T_LOGE(TAG, "Missing parameters: max_channels=%p, interval_ms=%p",
               max_channels_item, interval_ms_item);
        cJSON_Delete(root);
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_sendstr(req, "{\"error\":\"Missing parameters\"}");
        return ESP_FAIL;
    }

    uint8_t max_channels = (uint8_t)cJSON_GetNumberValue(max_channels_item);
    uint16_t interval_ms = (uint16_t)cJSON_GetNumberValue(interval_ms_item);
    cJSON_Delete(root);

    T_LOGD(TAG, "Parsed params: max_channels=%d, interval_ms=%d", max_channels, interval_ms);

    // 파라미터 검증
    if (max_channels < 1 || max_channels > 20) {
        T_LOGE(TAG, "Invalid max_channels: %d", max_channels);
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_sendstr(req, "{\"error\":\"max_channels must be 1-20\"}");
        return ESP_FAIL;
    }

    if (interval_ms < 100 || interval_ms > 3000) {
        T_LOGE(TAG, "Invalid interval_ms: %d", interval_ms);
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_sendstr(req, "{\"error\":\"interval_ms must be 100-3000\"}");
        return ESP_FAIL;
    }

    // 이벤트 발행
    tally_test_mode_config_t test_config = {
        .max_channels = max_channels,
        .interval_ms = interval_ms
    };
    event_bus_publish(EVT_TALLY_TEST_MODE_START, &test_config, sizeof(test_config));

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"started\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/test/stop - 테스트 모드 중지
 */
static esp_err_t api_test_stop_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // 이벤트 발행
    event_bus_publish(EVT_TALLY_TEST_MODE_STOP, nullptr, 0);

    // 응답
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"stopped\"}");

    return ESP_OK;
}

/**
 * @brief GET /api/lora/scan - 스캔 상태 및 결과 반환
 */
static esp_err_t api_lora_scan_get_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "scanning", s_cache.lora_scanning);
    cJSON_AddNumberToObject(root, "progress", s_cache.lora_scan_progress);

    // 스캔 결과
    cJSON* results = cJSON_CreateArray();
    if (s_cache.lora_scan_valid) {
        for (uint8_t i = 0; i < s_cache.lora_scan.count; i++) {
            cJSON* channel = cJSON_CreateObject();
            cJSON_AddNumberToObject(channel, "frequency", s_cache.lora_scan.channels[i].frequency);
            cJSON_AddNumberToObject(channel, "rssi", s_cache.lora_scan.channels[i].rssi);
            cJSON_AddNumberToObject(channel, "noiseFloor", s_cache.lora_scan.channels[i].noise_floor);
            cJSON_AddBoolToObject(channel, "clearChannel", s_cache.lora_scan.channels[i].clear_channel);
            cJSON_AddStringToObject(channel, "status", s_cache.lora_scan.channels[i].clear_channel ? "clear" : "busy");

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

/**
 * @brief POST /api/lora/scan/start - 스캔 시작
 */
static esp_err_t api_lora_scan_start_handler(httpd_req_t* req)
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

/**
 * @brief POST /api/lora/scan/stop - 스캔 중지
 */
static esp_err_t api_lora_scan_stop_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // 스캔 중지 이벤트 발행
    event_bus_publish(EVT_LORA_SCAN_STOP, nullptr, 0);

    s_cache.lora_scanning = false;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"stopped\"}");

    return ESP_OK;
}

/**
 * @brief GET /api/devices - 디바이스 리스트 반환 (TX 전용)
 */
static esp_err_t api_devices_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 디바이스 수
    cJSON_AddNumberToObject(root, "count", s_cache.devices.count);
    cJSON_AddNumberToObject(root, "registeredCount", s_cache.devices.registered_count);

    // 디바이스 배열
    cJSON* devices_array = cJSON_CreateArray();
    if (devices_array) {
        for (uint8_t i = 0; i < s_cache.devices.count; i++) {
            const device_info_t* dev = &s_cache.devices.devices[i];

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

/**
 * @brief DELETE /api/devices - 디바이스 삭제 (TX 전용)
 */
static esp_err_t api_delete_device_handler(httpd_req_t* req)
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
    T_LOGI(TAG, "디바이스 삭제 요청: %s", id_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/license/validate - 라이센스 키 검증 (이벤트 기반)
 */
static esp_err_t api_license_validate_handler(httpd_req_t* req)
{
    set_cors_headers(req);

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
    delete[] buf;
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 라이센스 키 추출
    cJSON* key_json = cJSON_GetObjectItem(root, "key");
    if (!key_json || !cJSON_IsString(key_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'key' field");
        return ESP_FAIL;
    }

    const char* key = key_json->valuestring;
    if (strlen(key) != 16) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid key length\"}");
        return ESP_OK;
    }

    // 라이센스 검증 이벤트 발행
    license_validate_event_t validate_req;
    strncpy(validate_req.key, key, 16);
    validate_req.key[16] = '\0';
    event_bus_publish(EVT_LICENSE_VALIDATE, &validate_req, sizeof(validate_req));

    cJSON_Delete(root);

    // 응답 (검증은 비동기로 처리됨, 상태는 EVT_LICENSE_STATE_CHANGED로 업데이트됨)
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"accepted\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/test/internet - 인터넷 연결 테스트 (8.8.8.8 핑)
 */
static esp_err_t api_test_internet_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    bool success = false;
    int ping_ms = 0;

    // 8.8.8.8 (Google DNS) 핑 테스트
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);  // DNS 포트
        addr.sin_addr.s_addr = esp_ip4addr_aton("8.8.8.8");

        // 타이머 시작
        int64_t start = esp_timer_get_time();

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            int64_t end = esp_timer_get_time();
            ping_ms = (end - start) / 1000;  // ms 변환
            success = true;
        }

        close(sock);
    }

    cJSON_AddBoolToObject(root, "success", success);
    if (success) {
        cJSON_AddNumberToObject(root, "ping", ping_ms);
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

/**
 * @brief POST /api/test/license-server - 라이센스 서버 연결 테스트 (프록시 통해)
 */
static esp_err_t api_test_license_server_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    bool success = false;
    int ping_ms = 0;

    // 프록시 서버 연결 테스트 (tally-node.duckdns.org:80)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        // 소켓 타임아웃 설정 (5초)
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);  // HTTP 포트

        // DNS 해석 (프록시 호스트)
        struct hostent* hp = gethostbyname("tally-node.duckdns.org");
        if (hp != nullptr && hp->h_addrtype == AF_INET && hp->h_length > 0) {
            memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));

            int64_t start = esp_timer_get_time();

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                int64_t end = esp_timer_get_time();
                ping_ms = (end - start) / 1000;
                success = true;
                T_LOGI(TAG, "License server test success: %d ms", ping_ms);
            } else {
                T_LOGW(TAG, "License server test: connect failed");
            }
        } else {
            T_LOGW(TAG, "License server test: DNS resolution failed");
        }

        close(sock);
    } else {
        T_LOGE(TAG, "License server test: socket creation failed");
    }

    cJSON_AddBoolToObject(root, "success", success);
    if (success) {
        cJSON_AddNumberToObject(root, "ping", ping_ms);
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

// ============================================================================
// 공지사항 API용 HTTP 이벤트 핸들러
// ============================================================================

typedef struct {
    char* buffer;
    size_t buffer_size;
    size_t bytes_written;
} http_response_context_t;

/**
 * @brief HTTP 응답 데이터 이벤트 핸들러
 * @param evt HTTP 클라이언트 이벤트
 * @return ESP_OK 항상 성공
 * @details esp_http_client의 데이터 수신 이벤트를 처리하여 버퍼에 저장합니다
 */
static esp_err_t http_notices_event_handler(esp_http_client_event_t *evt)
{
    http_response_context_t* ctx = (http_response_context_t*)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx->bytes_written < ctx->buffer_size) {
                size_t copy_len = evt->data_len;
                if (copy_len > ctx->buffer_size - ctx->bytes_written) {
                    copy_len = ctx->buffer_size - ctx->bytes_written;
                }
                memcpy(ctx->buffer + ctx->bytes_written, evt->data, copy_len);
                ctx->bytes_written += copy_len;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief GET /api/notices - 공지사항 조회 (duckdns 프록시)
 */
static esp_err_t api_notices_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // HTTP 응답 버퍼 (스택 오버플로우 방지: 힙 할당)
    char* response_buffer = (char*)malloc(2048);
    if (!response_buffer) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"notices\":[]}");
        return ESP_OK;
    }
    memset(response_buffer, 0, 2048);

    // 이벤트 핸들러용 컨텍스트
    http_response_context_t context = { response_buffer, 2047, 0 };

    // esp_http_client로 외부 API 호출
    esp_http_client_config_t config = {};
    config.url = "http://tally-node.duckdns.org/api/notices";
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 5000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.user_agent = "ESP32-Tally-Node";
    config.keep_alive_enable = true;
    config.event_handler = http_notices_event_handler;
    config.user_data = &context;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(response_buffer);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"notices\":[]}");
        return ESP_OK;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK && context.bytes_written > 0) {
        response_buffer[context.bytes_written] = '\0';
        T_LOGI(TAG, "공지사항 조회 성공: %d bytes", context.bytes_written);
    } else {
        if (err != ESP_OK) {
            T_LOGW(TAG, "공지사항 조회 실패: %s", esp_err_to_name(err));
        } else {
            T_LOGW(TAG, "응답 데이터 없음");
        }
        snprintf(response_buffer, 2048, "{\"success\":false,\"notices\":[]}");
    }

    esp_http_client_cleanup(client);

    // 클라이언트에게 전달
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response_buffer);

    free(response_buffer);
    return ESP_OK;
}

/**
 * @brief POST /api/device/brightness - 디바이스 밝기 설정 (LoRa 전송)
 */
static esp_err_t api_device_brightness_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "디바이스 밝기 설정 요청: ID[%02X%02X], 밝기=%d",
             device_id[0], device_id[1], brightness);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief POST /api/device/camera-id - 디바이스 카메라 ID 설정 (LoRa 전송)
 */
static esp_err_t api_device_camera_id_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "디바이스 카메라 ID 설정 요청: ID[%02X%02X], CameraID=%d",
             device_id[0], device_id[1], camera_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

#ifdef DEVICE_MODE_TX
/**
 * @brief 일괄 밝기 제어 핸들러 (TX → all RX Broadcast)
 * POST /api/brightness/broadcast
 * Body: {"brightness": 128} (0-255)
 */
static esp_err_t api_brightness_broadcast_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "일괄 밝기 제어 요청 (Broadcast): brightness=%d", brightness);

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

/**
 * @brief 디바이스 PING 핸들러
 * POST /api/device/ping
 * Body: {"deviceId": [0x2D, 0x20]}
 */
static esp_err_t api_device_ping_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "디바이스 PING 요청: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief 디바이스 기능 정지 핸들러
 * POST /api/device/stop
 * Body: {"deviceId": [0x2D, 0x20]}
 */
static esp_err_t api_device_stop_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "디바이스 기능 정지 요청: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief 디바이스 재부팅 핸들러
 * POST /api/device/reboot
 * Body: {"deviceId": [0x2D, 0x20]}
 */
static esp_err_t api_device_reboot_handler(httpd_req_t* req)
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

    T_LOGD(TAG, "디바이스 재부팅 요청: ID[%02X%02X]", device_id[0], device_id[1]);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief 상태 요청 핸들러
 * POST /api/device/status-request
 * 모든 RX 디바이스에 상태 요청 전송
 */
static esp_err_t api_status_request_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // 상태 요청 이벤트 발행
    event_bus_publish(EVT_STATUS_REQUEST, nullptr, 0);

    T_LOGD(TAG, "상태 요청 전송 (Broadcast)");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}
#endif // DEVICE_MODE_TX

// ============================================================================
// LED Colors Cache (이벤트로 업데이트)
// ============================================================================

static struct {
    bool initialized;
    struct { uint8_t r, g, b; } program;
    struct { uint8_t r, g, b; } preview;
    struct { uint8_t r, g, b; } off;
} s_led_colors_cache = {
    .initialized = false,
    .program = {255, 0, 0},
    .preview = {0, 255, 0},
    .off = {0, 0, 0}
};

/**
 * @brief LED 색상 변경 이벤트 핸들러 (캐시 업데이트)
 */
static esp_err_t on_led_colors_event(const event_data_t* event)
{
    if (event->type == EVT_LED_COLORS_CHANGED) {
        if (event->data_size >= sizeof(led_colors_event_t)) {
            const led_colors_event_t* colors = (const led_colors_event_t*)event->data;
            s_led_colors_cache.program.r = colors->program_r;
            s_led_colors_cache.program.g = colors->program_g;
            s_led_colors_cache.program.b = colors->program_b;
            s_led_colors_cache.preview.r = colors->preview_r;
            s_led_colors_cache.preview.g = colors->preview_g;
            s_led_colors_cache.preview.b = colors->preview_b;
            s_led_colors_cache.off.r = colors->off_r;
            s_led_colors_cache.off.g = colors->off_g;
            s_led_colors_cache.off.b = colors->off_b;
            s_led_colors_cache.initialized = true;
        }
    }
    return ESP_OK;
}

/**
 * @brief GET /api/led/colors - LED 색상 조회 (캐시 또는 요청 이벤트)
 */
static esp_err_t api_led_colors_get_handler(httpd_req_t* req)
{
    set_cors_headers(req);

    // 캐시가 없으면 요청 이벤트 발행 (config_service에서 응답)
    if (!s_led_colors_cache.initialized) {
        event_bus_publish(EVT_LED_COLORS_REQUEST, NULL, 0);
        // 응답 대기 (간단 구현을 위해 짧은 지연)
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    httpd_resp_set_type(req, "application/json");
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"program\":{\"r\":%d,\"g\":%d,\"b\":%d},"
             "\"preview\":{\"r\":%d,\"g\":%d,\"b\":%d},"
             "\"off\":{\"r\":%d,\"g\":%d,\"b\":%d}}",
             s_led_colors_cache.program.r, s_led_colors_cache.program.g, s_led_colors_cache.program.b,
             s_led_colors_cache.preview.r, s_led_colors_cache.preview.g, s_led_colors_cache.preview.b,
             s_led_colors_cache.off.r, s_led_colors_cache.off.g, s_led_colors_cache.off.b);
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

/**
 * @brief POST /api/led/colors - LED 색상 설정
 */
static esp_err_t api_led_colors_post_handler(httpd_req_t* req)
{
    set_cors_headers(req);

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
    delete[] buf;

    if (root == nullptr) {
        T_LOGE(TAG, "POST /api/led/colors JSON 파싱 실패");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 정적 구조체로 이벤트 발행
    static led_colors_event_t colors;

    // 기본값 (기존 색상 유지)
    cJSON* program = cJSON_GetObjectItem(root, "program");
    cJSON* preview = cJSON_GetObjectItem(root, "preview");
    cJSON* off = cJSON_GetObjectItem(root, "off");

    if (program) {
        cJSON* r = cJSON_GetObjectItem(program, "r");
        cJSON* g = cJSON_GetObjectItem(program, "g");
        cJSON* b = cJSON_GetObjectItem(program, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.program_r = (uint8_t)r->valueint;
            colors.program_g = (uint8_t)g->valueint;
            colors.program_b = (uint8_t)b->valueint;
        }
    }

    if (preview) {
        cJSON* r = cJSON_GetObjectItem(preview, "r");
        cJSON* g = cJSON_GetObjectItem(preview, "g");
        cJSON* b = cJSON_GetObjectItem(preview, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.preview_r = (uint8_t)r->valueint;
            colors.preview_g = (uint8_t)g->valueint;
            colors.preview_b = (uint8_t)b->valueint;
        }
    }

    if (off) {
        cJSON* r = cJSON_GetObjectItem(off, "r");
        cJSON* g = cJSON_GetObjectItem(off, "g");
        cJSON* b = cJSON_GetObjectItem(off, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.off_r = (uint8_t)r->valueint;
            colors.off_g = (uint8_t)g->valueint;
            colors.off_b = (uint8_t)b->valueint;
        }
    }

    cJSON_Delete(root);

    // 색상 변경 이벤트 발행 (config_service에서 구독)
    event_bus_publish(EVT_LED_COLORS_CHANGED, &colors, sizeof(colors));

    T_LOGI(TAG, "LED colors changed: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d)",
             colors.program_r, colors.program_g, colors.program_b,
             colors.preview_r, colors.preview_g, colors.preview_b,
             colors.off_r, colors.off_g, colors.off_b);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

/**
 * @brief 인덱스 HTML 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details 임베디드된 index.html 파일을 응답으로 전송합니다
 */
static esp_err_t index_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_data, index_html_len);
    return ESP_OK;
}

/**
 * @brief CSS 파일 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details 임베디드된 styles.css 파일을 응답으로 전송합니다
 */
static esp_err_t css_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char*)styles_css_data, styles_css_len);
    return ESP_OK;
}

/**
 * @brief JS 파일 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details 임베디드된 app.bundle.js 파일을 응답으로 전송합니다
 */
static esp_err_t js_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char*)app_bundle_js_data, app_bundle_js_len);
    return ESP_OK;
}

/**
 * @brief Alpine.js 파일 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details 임베디드된 alpine.js 파일을 응답으로 전송합니다
 */
static esp_err_t alpine_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char*)alpine_js_data, alpine_js_len);
    return ESP_OK;
}

/**
 * @brief Favicon 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details 빈 응답(204 No Content)을 반환하여 404 에러를 방지합니다
 */
static esp_err_t favicon_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief CORS Preflight 핸들러
 * @param req HTTP 요청 핸들러
 * @return ESP_OK 항상 성공
 * @details OPTIONS 요청에 대해 CORS 헤더를 설정하고 204 응답을 반환합니다
 */
static esp_err_t options_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ============================================================================
// URI 등록
// ============================================================================

static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_status = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = api_status_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_POST,
    .handler = api_reboot_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_reboot_broadcast = {
    .uri = "/api/reboot/broadcast",
    .method = HTTP_POST,
    .handler = api_reboot_broadcast_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_test_start = {
    .uri = "/api/test/start",
    .method = HTTP_POST,
    .handler = api_test_start_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_test_stop = {
    .uri = "/api/test/stop",
    .method = HTTP_POST,
    .handler = api_test_stop_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_ap = {
    .uri = "/api/config/network/ap",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_wifi = {
    .uri = "/api/config/network/wifi",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_network_ethernet = {
    .uri = "/api/config/network/ethernet",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_switcher_primary = {
    .uri = "/api/config/switcher/primary",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_switcher_secondary = {
    .uri = "/api/config/switcher/secondary",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_config_switcher_dual = {
    .uri = "/api/config/switcher/dual",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

// Broadcast API URI (RF 설정만)
static const httpd_uri_t uri_api_config_device_rf = {
    .uri = "/api/config/device/rf",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

// LoRa API URI (internal)
static const httpd_uri_t uri_api_lora_scan = {
    .uri = "/api/lora/scan",
    .method = HTTP_GET,
    .handler = api_lora_scan_get_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_lora_scan_start = {
    .uri = "/api/lora/scan/start",
    .method = HTTP_POST,
    .handler = api_lora_scan_start_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_lora_scan_stop = {
    .uri = "/api/lora/scan/stop",
    .method = HTTP_POST,
    .handler = api_lora_scan_stop_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_devices = {
    .uri = "/api/devices",
    .method = HTTP_GET,
    .handler = api_devices_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_delete_device = {
    .uri = "/api/devices",
    .method = HTTP_DELETE,
    .handler = api_delete_device_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_license_validate = {
    .uri = "/api/validate-license",
    .method = HTTP_POST,
    .handler = api_license_validate_handler,
    .user_ctx = nullptr
};

// Test API URI
static const httpd_uri_t uri_api_test_internet = {
    .uri = "/api/test/internet",
    .method = HTTP_POST,
    .handler = api_test_internet_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_test_license_server = {
    .uri = "/api/test/license-server",
    .method = HTTP_POST,
    .handler = api_test_license_server_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_notices = {
    .uri = "/api/notices",
    .method = HTTP_GET,
    .handler = api_notices_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_device_brightness = {
    .uri = "/api/device/brightness",
    .method = HTTP_POST,
    .handler = api_device_brightness_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_device_camera_id = {
    .uri = "/api/device/camera-id",
    .method = HTTP_POST,
    .handler = api_device_camera_id_handler,
    .user_ctx = nullptr
};

#ifdef DEVICE_MODE_TX
static const httpd_uri_t uri_api_brightness_broadcast = {
    .uri = "/api/brightness/broadcast",
    .method = HTTP_POST,
    .handler = api_brightness_broadcast_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_device_ping = {
    .uri = "/api/device/ping",
    .method = HTTP_POST,
    .handler = api_device_ping_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_device_stop = {
    .uri = "/api/device/stop",
    .method = HTTP_POST,
    .handler = api_device_stop_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_device_reboot = {
    .uri = "/api/device/reboot",
    .method = HTTP_POST,
    .handler = api_device_reboot_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_status_request = {
    .uri = "/api/device/status-request",
    .method = HTTP_POST,
    .handler = api_status_request_handler,
    .user_ctx = nullptr
};

// LED 색상 API
static const httpd_uri_t uri_api_led_colors_get = {
    .uri = "/api/led/colors",
    .method = HTTP_GET,
    .handler = api_led_colors_get_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_api_led_colors_post = {
    .uri = "/api/led/colors",
    .method = HTTP_POST,
    .handler = api_led_colors_post_handler,
    .user_ctx = nullptr
};
#endif

// 정적 파일 URI
static const httpd_uri_t uri_css = {
    .uri = "/css/styles.css",
    .method = HTTP_GET,
    .handler = css_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_js = {
    .uri = "/js/app.bundle.js",
    .method = HTTP_GET,
    .handler = js_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_alpine = {
    .uri = "/vendor/alpine.js",
    .method = HTTP_GET,
    .handler = alpine_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_handler,
    .user_ctx = nullptr
};

// CORS Preflight URI (OPTIONS)
static const httpd_uri_t uri_options_api_status = {
    .uri = "/api/status",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_reboot_broadcast = {
    .uri = "/api/reboot/broadcast",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_test_start = {
    .uri = "/api/test/start",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_test_stop = {
    .uri = "/api/test/stop",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_config = {
    .uri = "/api/config/*",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_lora = {
    .uri = "/api/lora/*",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_devices = {
    .uri = "/api/devices",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_license_validate = {
    .uri = "/api/validate-license",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_test_internet = {
    .uri = "/api/test/internet",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_test_license_server = {
    .uri = "/api/test/license-server",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_test = {
    .uri = "/api/test/*",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_notices = {
    .uri = "/api/notices",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_device_brightness = {
    .uri = "/api/device/brightness",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_device_camera_id = {
    .uri = "/api/device/camera-id",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

#ifdef DEVICE_MODE_TX
static const httpd_uri_t uri_options_api_brightness_broadcast = {
    .uri = "/api/brightness/broadcast",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_device_ping = {
    .uri = "/api/device/ping",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_device_stop = {
    .uri = "/api/device/stop",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_device_reboot = {
    .uri = "/api/device/reboot",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_status_request = {
    .uri = "/api/device/status-request",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};
#endif

// LED 색상 API OPTIONS
static const httpd_uri_t uri_options_api_led_colors = {
    .uri = "/api/led/colors",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

// ============================================================================
// C 인터페이스
// ============================================================================

extern "C" {

static bool s_initialized = false;

/**
 * @brief 웹 서버 초기화
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 이미 초기화됨
 * @details 캐시를 초기화하고 이벤트 버스에 필요한 이벤트 핸들러들을 구독합니다
 */
esp_err_t web_server_init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "Web server already initialized");
        return ESP_OK;
    }

    // 캐시 초기화
    init_cache();

    // 이벤트 구독
    event_bus_subscribe(EVT_INFO_UPDATED, onSystemInfoEvent);
    event_bus_subscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_subscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);
    // LoRa 스캔 이벤트
    event_bus_subscribe(EVT_LORA_SCAN_START, onLoraScanStartEvent);
    event_bus_subscribe(EVT_LORA_SCAN_PROGRESS, onLoraScanProgressEvent);
    event_bus_subscribe(EVT_LORA_SCAN_COMPLETE, onLoraScanCompleteEvent);
    // 디바이스 리스트 이벤트 (TX 전용)
    event_bus_subscribe(EVT_DEVICE_LIST_CHANGED, onDeviceListEvent);
    // 라이센스 상태 이벤트
    event_bus_subscribe(EVT_LICENSE_STATE_CHANGED, onLicenseStateEvent);
    // 네트워크 재시작 완료 이벤트
    event_bus_subscribe(EVT_NETWORK_RESTARTED, onNetworkRestartedEvent);
    // LED 색상 이벤트
    event_bus_subscribe(EVT_LED_COLORS_CHANGED, on_led_colors_event);

    s_initialized = true;
    T_LOGI(TAG, "Web server initialized (event subscriptions ready)");
    return ESP_OK;
}

/**
 * @brief 웹 서버 시작
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음, ESP_FAIL HTTP 서버 시작 실패
 * @details HTTP 서버를 시작하고 모든 API 핸들러를 등록합니다
 */
esp_err_t web_server_start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Web server not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_server != nullptr) {
        T_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 10;  // 5 → 10 (동시 연결 증가)
    config.max_uri_handlers = 48;  // API + OPTIONS + 정적 파일 + LoRa + Device + License + Notices + LED
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return ret;
    }

    // URI 등록
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_css);
    httpd_register_uri_handler(s_server, &uri_js);
    httpd_register_uri_handler(s_server, &uri_alpine);
    httpd_register_uri_handler(s_server, &uri_favicon);
    httpd_register_uri_handler(s_server, &uri_api_status);
    httpd_register_uri_handler(s_server, &uri_api_reboot);
    httpd_register_uri_handler(s_server, &uri_api_reboot_broadcast);
    httpd_register_uri_handler(s_server, &uri_api_config_network_ap);
    httpd_register_uri_handler(s_server, &uri_api_config_network_wifi);
    httpd_register_uri_handler(s_server, &uri_api_config_network_ethernet);
    httpd_register_uri_handler(s_server, &uri_api_config_switcher_primary);
    httpd_register_uri_handler(s_server, &uri_api_config_switcher_secondary);
    httpd_register_uri_handler(s_server, &uri_api_config_switcher_dual);
    httpd_register_uri_handler(s_server, &uri_api_config_device_rf);
    // LoRa API (internal)
    httpd_register_uri_handler(s_server, &uri_api_lora_scan);
    httpd_register_uri_handler(s_server, &uri_api_lora_scan_start);
    httpd_register_uri_handler(s_server, &uri_api_lora_scan_stop);
    // Device API (TX only)
    httpd_register_uri_handler(s_server, &uri_api_devices);
    httpd_register_uri_handler(s_server, &uri_api_delete_device);
    // License API
    httpd_register_uri_handler(s_server, &uri_api_license_validate);
    // Test API
    httpd_register_uri_handler(s_server, &uri_api_test_internet);
    httpd_register_uri_handler(s_server, &uri_api_test_license_server);
    // Tally Test Mode API
    httpd_register_uri_handler(s_server, &uri_api_test_start);
    httpd_register_uri_handler(s_server, &uri_api_test_stop);
    // Notices API
    httpd_register_uri_handler(s_server, &uri_api_notices);
    // Device Brightness API
    httpd_register_uri_handler(s_server, &uri_api_device_brightness);
    // Device Camera ID API
    httpd_register_uri_handler(s_server, &uri_api_device_camera_id);
    // LED Colors API
    httpd_register_uri_handler(s_server, &uri_api_led_colors_get);
    httpd_register_uri_handler(s_server, &uri_api_led_colors_post);
#ifdef DEVICE_MODE_TX
    // Global Brightness Broadcast API
    httpd_register_uri_handler(s_server, &uri_api_brightness_broadcast);
    // Device PING API
    httpd_register_uri_handler(s_server, &uri_api_device_ping);
    // Device STOP API
    httpd_register_uri_handler(s_server, &uri_api_device_stop);
    // Device REBOOT API
    httpd_register_uri_handler(s_server, &uri_api_device_reboot);
    // Status Request API
    httpd_register_uri_handler(s_server, &uri_api_status_request);
#endif
    // CORS Preflight (OPTIONS)
    httpd_register_uri_handler(s_server, &uri_options_api_status);
    httpd_register_uri_handler(s_server, &uri_options_api_reboot);
    httpd_register_uri_handler(s_server, &uri_options_api_reboot_broadcast);
    httpd_register_uri_handler(s_server, &uri_options_api_config);
    httpd_register_uri_handler(s_server, &uri_options_api_lora);
    httpd_register_uri_handler(s_server, &uri_options_api_devices);
    httpd_register_uri_handler(s_server, &uri_options_api_license_validate);
    httpd_register_uri_handler(s_server, &uri_options_api_test_internet);
    httpd_register_uri_handler(s_server, &uri_options_api_test_license_server);
    httpd_register_uri_handler(s_server, &uri_options_api_test);
    httpd_register_uri_handler(s_server, &uri_options_api_test_start);
    httpd_register_uri_handler(s_server, &uri_options_api_test_stop);
    httpd_register_uri_handler(s_server, &uri_options_api_notices);
    httpd_register_uri_handler(s_server, &uri_options_api_device_brightness);
    httpd_register_uri_handler(s_server, &uri_options_api_device_camera_id);
    // LED Colors OPTIONS
    httpd_register_uri_handler(s_server, &uri_options_api_led_colors);
#ifdef DEVICE_MODE_TX
    httpd_register_uri_handler(s_server, &uri_options_api_brightness_broadcast);
    httpd_register_uri_handler(s_server, &uri_options_api_device_ping);
    httpd_register_uri_handler(s_server, &uri_options_api_device_stop);
    httpd_register_uri_handler(s_server, &uri_options_api_device_reboot);
    httpd_register_uri_handler(s_server, &uri_options_api_status_request);
#endif

    // 설정 데이터 요청 (초기 캐시 populate)
    event_bus_publish(EVT_CONFIG_DATA_REQUEST, nullptr, 0);

    T_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

/**
 * @brief 웹 서버 중지
 * @return ESP_OK 항상 성공
 * @details HTTP 서버를 중지하고 이벤트 구독을 해제하며 캐시를 정리합니다
 */
esp_err_t web_server_stop(void)
{
    if (s_server == nullptr) {
        return ESP_OK;
    }

    T_LOGI(TAG, "Stopping web server");

    esp_err_t ret = httpd_stop(s_server);
    s_server = nullptr;

    // 이벤트 구독 해제
    event_bus_unsubscribe(EVT_INFO_UPDATED, onSystemInfoEvent);
    event_bus_unsubscribe(EVT_SWITCHER_STATUS_CHANGED, onSwitcherStatusEvent);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);
    // LoRa 스캔 이벤트 해제
    event_bus_unsubscribe(EVT_LORA_SCAN_START, onLoraScanStartEvent);
    event_bus_unsubscribe(EVT_LORA_SCAN_PROGRESS, onLoraScanProgressEvent);
    event_bus_unsubscribe(EVT_LORA_SCAN_COMPLETE, onLoraScanCompleteEvent);
    // 디바이스 리스트 이벤트 해제
    event_bus_unsubscribe(EVT_DEVICE_LIST_CHANGED, onDeviceListEvent);
    // 라이센스 상태 이벤트 해제
    event_bus_unsubscribe(EVT_LICENSE_STATE_CHANGED, onLicenseStateEvent);

    // 캐시 무효화
    s_cache.system_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.config_valid = false;
    s_cache.devices_valid = false;
    s_cache.license_valid = false;

    // 뮤텍스 해제
    if (s_cache_mutex) {
        vSemaphoreDelete(s_cache_mutex);
        s_cache_mutex = nullptr;
    }

    s_initialized = false;
    return ret;
}

/**
 * @brief 웹 서버 실행 상태 확인
 * @return true 서버 실행 중, false 서버 중지됨
 */
bool web_server_is_running(void)
{
    return s_server != nullptr;
}

} // extern "C"
