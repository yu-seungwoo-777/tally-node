/**
 * @file web_server_events.cpp
 * @brief Web Server 이벤트 핸들러 구현
 */

#include "web_server_events.h"
#include "web_server_cache.h"
#include "web_server.h"
#include "esp_log.h"
#include "t_log.h"
#include "string.h"

static const char* TAG = "02_WebSvr_Events";

extern "C" {

// ============================================================================
// 이벤트 핸들러 구현
// ============================================================================

esp_err_t web_server_on_system_info_event(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (event->data_size < sizeof(system_info_event_t)) {
        T_LOGE(TAG, "System info: invalid data size %d (expected %zu)",
                 event->data_size, sizeof(system_info_event_t));
        return ESP_ERR_INVALID_ARG;
    }

    const system_info_event_t* info = (const system_info_event_t*)event->data;
    web_server_cache_update_system(info);

    return ESP_OK;
}

esp_err_t web_server_on_switcher_status_event(const event_data_t* event)
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
    web_server_cache_update_switcher(status);

    return ESP_OK;
}

esp_err_t web_server_on_network_status_event(const event_data_t* event)
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
    web_server_cache_update_network(status);

    return ESP_OK;
}

esp_err_t web_server_on_config_data_event(const event_data_t* event)
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
    web_server_cache_update_config(config);

    return ESP_OK;
}

esp_err_t web_server_on_lora_scan_start_event(const event_data_t* event)
{
    (void)event;
    web_server_cache_set_lora_scan_starting();
    return ESP_OK;
}

esp_err_t web_server_on_lora_scan_progress_event(const event_data_t* event)
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
    web_server_cache_update_lora_scan_progress(progress);

    return ESP_OK;
}

esp_err_t web_server_on_lora_scan_complete_event(const event_data_t* event)
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
    web_server_cache_update_lora_scan_complete(result);

    return ESP_OK;
}

esp_err_t web_server_on_device_list_event(const event_data_t* event)
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
    web_server_cache_update_devices(devices);

    return ESP_OK;
}

esp_err_t web_server_on_license_state_event(const event_data_t* event)
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
    web_server_cache_update_license(license);

    return ESP_OK;
}

esp_err_t web_server_on_network_restarted_event(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "Network restart complete - restarting web server");

    // 웹서버가 실행 중이면 재시작
    if (web_server_is_running()) {
        web_server_stop();
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 대기
    }

    // 웹서버 재시작
    return web_server_start();
}

esp_err_t web_server_on_led_colors_event(const event_data_t* event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    if (event->type == EVT_LED_COLORS_CHANGED) {
        if (event->data_size >= sizeof(led_colors_event_t)) {
            const led_colors_event_t* colors = (const led_colors_event_t*)event->data;
            web_server_cache_update_led_colors(colors);
        }
    }
    return ESP_OK;
}

} // extern "C"
