/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API (Refactored)
 */

#include "web_server.h"
#include "web_server_cache.h"
#include "web_server_events.h"
#include "web_server_json.h"
#include "web_server_config.h"
#include "web_server_routes.h"
#include "web_server_helpers.h"
#include "event_bus.h"
#include "esp_http_server.h"
#include "t_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "02_WS";
static httpd_handle_t s_server = nullptr;

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
    web_server_cache_init();

    // 이벤트 구독 (이벤트 핸들러 모듈 사용)
    event_bus_subscribe(EVT_INFO_UPDATED, web_server_on_system_info_event);
    event_bus_subscribe(EVT_SWITCHER_STATUS_CHANGED, web_server_on_switcher_status_event);
    event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, web_server_on_network_status_event);
    event_bus_subscribe(EVT_CONFIG_DATA_CHANGED, web_server_on_config_data_event);
    // LoRa 스캔 이벤트
    event_bus_subscribe(EVT_LORA_SCAN_START, web_server_on_lora_scan_start_event);
    event_bus_subscribe(EVT_LORA_SCAN_PROGRESS, web_server_on_lora_scan_progress_event);
    event_bus_subscribe(EVT_LORA_SCAN_COMPLETE, web_server_on_lora_scan_complete_event);
    // 디바이스 리스트 이벤트 (TX 전용)
    event_bus_subscribe(EVT_DEVICE_LIST_CHANGED, web_server_on_device_list_event);
    // 라이센스 상태 이벤트
    event_bus_subscribe(EVT_LICENSE_STATE_CHANGED, web_server_on_license_state_event);
    // 네트워크 재시작 완료 이벤트
    event_bus_subscribe(EVT_NETWORK_RESTARTED, web_server_on_network_restarted_event);
    // LED 색상 이벤트
    event_bus_subscribe(EVT_LED_COLORS_CHANGED, web_server_on_led_colors_event);

    s_initialized = true;

    // 부팅 시 NVS에서 LED 색상 로드 (config_service에서 응답)
    event_bus_publish(EVT_LED_COLORS_REQUEST, NULL, 0);

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
    config.max_open_sockets = 7;  // LWIP_MAX_SOCKETS=10일 때 최대값 (10-3=7)
    config.max_uri_handlers = g_route_count + 4;  // 여유분 확보
    config.stack_size = 12288;  // 스택 오버플로우 방지 (8KB → 12KB, cJSON 생성 시 여유)
    config.task_priority = 5;  // 3→5 (event_bus와 동일, CPU 시간 확보)
    config.lru_purge_enable = true;
    // Keep-Alive 설정 (단일 사용자 최적화)
    config.keep_alive_enable = true;
    config.keep_alive_idle = 30;     // 30초 동안 유휴시 연결 유지
    config.keep_alive_interval = 5;  // 5초마다 keep-alive 프로브
    config.keep_alive_count = 3;     // 3회 실패시 연결 종료

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return ret;
    }

    // 자동 라우트 등록
    for (size_t i = 0; i < g_route_count; i++) {
        ret = httpd_register_uri_handler(s_server, &g_routes[i]);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "Failed to register route %zu: %s", i, g_routes[i].uri);
        }
    }

    T_LOGI(TAG, "Web server started on port 80 (registered %zu routes)", g_route_count);

    // 설정 데이터 요청 (초기 캐시 populate)
    event_bus_publish(EVT_CONFIG_DATA_REQUEST, nullptr, 0);

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
    event_bus_unsubscribe(EVT_INFO_UPDATED, web_server_on_system_info_event);
    event_bus_unsubscribe(EVT_SWITCHER_STATUS_CHANGED, web_server_on_switcher_status_event);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, web_server_on_network_status_event);
    event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, web_server_on_config_data_event);
    event_bus_unsubscribe(EVT_LORA_SCAN_START, web_server_on_lora_scan_start_event);
    event_bus_unsubscribe(EVT_LORA_SCAN_PROGRESS, web_server_on_lora_scan_progress_event);
    event_bus_unsubscribe(EVT_LORA_SCAN_COMPLETE, web_server_on_lora_scan_complete_event);
    event_bus_unsubscribe(EVT_DEVICE_LIST_CHANGED, web_server_on_device_list_event);
    event_bus_unsubscribe(EVT_LICENSE_STATE_CHANGED, web_server_on_license_state_event);

    // 캐시 정리
    web_server_cache_invalidate();
    web_server_cache_deinit();

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
