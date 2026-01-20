/**
 * @file web_server_events.cpp
 * @brief Web Server 이벤트 핸들러 구현
 */

#include "web_server_events.h"
#include "web_server_cache.h"
#include "web_server.h"
#include "t_log.h"
#include "error_macros.h"
#include "string.h"

static const char* TAG = "02_WS_Events";

// ============================================================================
// 이벤트 크기 검증 매크로
// ============================================================================

#define VALIDATE_EVENT_SIZE(event, type_t, name) \
    do { \
        if ((event)->data_size < sizeof(type_t)) { \
            T_LOGE(TAG, "%s: invalid size %d (expected %zu)", \
                    name, (event)->data_size, sizeof(type_t)); \
            return ESP_ERR_INVALID_ARG; \
        } \
    } while(0)

extern "C" {

// ============================================================================
// 이벤트 핸들러 구현
// ============================================================================

esp_err_t web_server_on_system_info_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, system_info_event_t, "System info");

    const system_info_event_t* info = (const system_info_event_t*)event->data;
    web_server_cache_update_system(info);

    return ESP_OK;
}

esp_err_t web_server_on_switcher_status_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, switcher_status_event_t, "Switcher status");

    const switcher_status_event_t* status = (const switcher_status_event_t*)event->data;
    web_server_cache_update_switcher(status);

    return ESP_OK;
}

esp_err_t web_server_on_network_status_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, network_status_event_t, "Network status");

    const network_status_event_t* status = (const network_status_event_t*)event->data;
    web_server_cache_update_network(status);

    return ESP_OK;
}

esp_err_t web_server_on_config_data_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, config_data_event_t, "Config data");

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
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, lora_scan_progress_t, "LoRa scan progress");

    const lora_scan_progress_t* progress = (const lora_scan_progress_t*)event->data;
    web_server_cache_update_lora_scan_progress(progress);

    return ESP_OK;
}

esp_err_t web_server_on_lora_scan_complete_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, lora_scan_complete_t, "LoRa scan complete");

    const lora_scan_complete_t* result = (const lora_scan_complete_t*)event->data;
    web_server_cache_update_lora_scan_complete(result);

    return ESP_OK;
}

esp_err_t web_server_on_device_list_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, device_list_event_t, "Device list");

    const device_list_event_t* devices = (const device_list_event_t*)event->data;
    web_server_cache_update_devices(devices);

    return ESP_OK;
}

esp_err_t web_server_on_license_state_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);
    VALIDATE_EVENT_SIZE(event, license_state_event_t, "License state");

    const license_state_event_t* license = (const license_state_event_t*)event->data;
    web_server_cache_update_license(license);

    return ESP_OK;
}

esp_err_t web_server_on_network_restarted_event(const event_data_t* event)
{
    (void)event;
    T_LOGI(TAG, "Network restart complete - web server continues running");

    // 네트워크 재시작 후에도 웹 서버는 계속 실행 상태 유지
    // ESP-IDF httpd는 네트워크 변경 시 자동으로 연결을 재설정함
    // 명시적인 재시작 제거: 진행 중인 요청이 중단되는 문제 방지

    // 캐시 무효화하여 새 네트워크 상태로 갱신 유도
    web_server_cache_invalidate();

    // 설정 데이터 재요청하여 캐시 갱신
    event_bus_publish(EVT_CONFIG_DATA_REQUEST, nullptr, 0);

    return ESP_OK;
}

esp_err_t web_server_on_led_colors_event(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    if (event->type == EVT_LED_COLORS_CHANGED) {
        if (event->data_size >= sizeof(led_colors_event_t)) {
            const led_colors_event_t* colors = (const led_colors_event_t*)event->data;
            web_server_cache_update_led_colors(colors);
        }
    }
    return ESP_OK;
}

} // extern "C"
