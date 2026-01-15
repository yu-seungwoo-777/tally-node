/**
 * @file web_server_cache.cpp
 * @brief Web Server 내부 데이터 캐시 구현
 */

#include "web_server_cache.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "string.h"

static const char* TAG = "02_WS_Cache";

// ============================================================================
// 내부 데이터 캐시
// ============================================================================

static web_server_data_t s_cache;
static SemaphoreHandle_t s_cache_mutex = nullptr;  // s_cache 보호용 뮤텍스

// ============================================================================
// LED 색상 캐시
// ============================================================================

static web_server_led_colors_t s_led_colors_cache = {
    .initialized = false,
    .program = {255, 0, 0},
    .preview = {0, 255, 0},
    .off = {0, 0, 0}
};

// ============================================================================
// 캐시 초기화 함수
// ============================================================================

void web_server_cache_init(void)
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

BaseType_t web_server_cache_lock(void)
{
    if (s_cache_mutex == nullptr) {
        return pdFALSE;
    }
    // 타임아웃 0: 즉시 획득 가능할 때만 시도 (이벤트 콜백에서 블록 방지)
    return xSemaphoreTake(s_cache_mutex, 0);
}

void web_server_cache_unlock(void)
{
    if (s_cache_mutex != nullptr) {
        xSemaphoreGive(s_cache_mutex);
    }
}

void web_server_cache_invalidate(void)
{
    s_cache.system_valid = false;
    s_cache.switcher_valid = false;
    s_cache.network_valid = false;
    s_cache.config_valid = false;
    s_cache.devices_valid = false;
    s_cache.license_valid = false;
}

void web_server_cache_deinit(void)
{
    if (s_cache_mutex) {
        vSemaphoreDelete(s_cache_mutex);
        s_cache_mutex = nullptr;
    }
}

// ============================================================================
// 캐시 데이터 업데이트 함수
// ============================================================================

void web_server_cache_update_system(const system_info_event_t* info)
{
    if (web_server_cache_lock() == pdTRUE) {
        memcpy(&s_cache.system, info, sizeof(system_info_event_t));
        s_cache.system_valid = true;
        web_server_cache_unlock();
    }
}

void web_server_cache_update_switcher(const switcher_status_event_t* status)
{
    if (web_server_cache_lock() == pdTRUE) {
        memcpy(&s_cache.switcher, status, sizeof(switcher_status_event_t));
        s_cache.switcher_valid = true;
        web_server_cache_unlock();
    }
}

void web_server_cache_update_network(const network_status_event_t* status)
{
    if (web_server_cache_lock() == pdTRUE) {
        memcpy(&s_cache.network, status, sizeof(network_status_event_t));
        s_cache.network_valid = true;
        web_server_cache_unlock();
    }
}

void web_server_cache_update_config(const config_data_event_t* config)
{
    if (web_server_cache_lock() == pdTRUE) {
        s_cache.config = *config;
        s_cache.config_valid = true;
        web_server_cache_unlock();
    }
}

void web_server_cache_set_lora_scan_starting(void)
{
    if (web_server_cache_lock() == pdTRUE) {
        s_cache.lora_scanning = true;
        s_cache.lora_scan_progress = 0;
        s_cache.lora_scan_valid = false;
        s_cache.lora_scan.count = 0;  // 이전 결과 초기화
        web_server_cache_unlock();
    }
}

void web_server_cache_update_lora_scan_progress(const lora_scan_progress_t* progress)
{
    if (web_server_cache_lock() == pdTRUE) {
        s_cache.lora_scan_progress = progress->progress;

        // 진행 중인 채널 결과 추가 (누적) - 버퍼 오버플로우 방지
        if (s_cache.lora_scan.count < 100) {
            s_cache.lora_scan.channels[s_cache.lora_scan.count] = progress->result;
            s_cache.lora_scan.count++;
            s_cache.lora_scan_valid = true;
        } else {
            T_LOGW(TAG, "LoRa scan progress: channel buffer full (100), ignoring");
        }
        web_server_cache_unlock();
    }
}

void web_server_cache_update_lora_scan_complete(const lora_scan_complete_t* result)
{
    if (web_server_cache_lock() == pdTRUE) {
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
        web_server_cache_unlock();
    }
}

void web_server_cache_update_devices(const device_list_event_t* devices_list)
{
    if (web_server_cache_lock() == pdTRUE) {
        // count 유효성 검증 (최대 20 디바이스)
        if (devices_list->count > 20) {
            T_LOGW(TAG, "Device list: count=%d exceeds limit, clamping to 20", devices_list->count);
            memcpy(&s_cache.devices, devices_list, sizeof(device_list_event_t));
            s_cache.devices.count = 20;
        } else {
            memcpy(&s_cache.devices, devices_list, sizeof(device_list_event_t));
        }
        s_cache.devices_valid = true;
        web_server_cache_unlock();
    }

    T_LOGD(TAG, "Device list updated: %d devices (registered: %d)",
             devices_list->count, devices_list->registered_count);
}

void web_server_cache_update_license(const license_state_event_t* license)
{
    if (web_server_cache_lock() == pdTRUE) {
        memcpy(&s_cache.license, license, sizeof(license_state_event_t));
        s_cache.license_valid = true;
        web_server_cache_unlock();
    }

    T_LOGD(TAG, "License state updated: limit=%d, state=%d",
             license->device_limit, license->state);
}

void web_server_cache_set_lora_scan_stopped(void)
{
    s_cache.lora_scanning = false;
}

// ============================================================================
// 캐시 데이터 읽기 함수
// ============================================================================

const web_server_data_t* web_server_cache_get(void)
{
    return &s_cache;
}

bool web_server_cache_is_system_valid(void)
{
    return s_cache.system_valid;
}

bool web_server_cache_is_switcher_valid(void)
{
    return s_cache.switcher_valid;
}

bool web_server_cache_is_network_valid(void)
{
    return s_cache.network_valid;
}

bool web_server_cache_is_config_valid(void)
{
    return s_cache.config_valid;
}

bool web_server_cache_is_lora_scan_valid(void)
{
    return s_cache.lora_scan_valid;
}

bool web_server_cache_is_lora_scanning(void)
{
    return s_cache.lora_scanning;
}

uint8_t web_server_cache_get_lora_scan_progress(void)
{
    return s_cache.lora_scan_progress;
}

bool web_server_cache_is_devices_valid(void)
{
    return s_cache.devices_valid;
}

bool web_server_cache_is_license_valid(void)
{
    return s_cache.license_valid;
}

// ============================================================================
// LED 색상 캐시 함수
// ============================================================================

bool web_server_cache_is_led_colors_initialized(void)
{
    return s_led_colors_cache.initialized;
}

void web_server_cache_update_led_colors(const led_colors_event_t* colors)
{
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

const web_server_led_colors_t* web_server_cache_get_led_colors(void)
{
    return &s_led_colors_cache;
}
