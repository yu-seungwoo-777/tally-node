/**
 * @file web_server.cpp
 * @brief Web Server Implementation - REST API (Refactored)
 */

#include "web_server.h"
#include "web_server_cache.h"
#include "web_server_events.h"
#include "web_server_json.h"
#include "web_server_config.h"
#include "handlers/api_status.h"
#include "handlers/api_config.h"
#include "handlers/api_devices.h"
#include "handlers/api_lora.h"
#include "handlers/api_test.h"
#include "handlers/api_led.h"
#include "handlers/api_license.h"
#include "handlers/api_notices.h"
#include "handlers/api_static.h"
#include "event_bus.h"
#include "esp_http_server.h"
#include "t_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

// 정적 파일 (임베디드)
#include "static_files.h"

static const char* TAG = "02_WebSvr";
static httpd_handle_t s_server = nullptr;

// ============================================================================
// URI 라우팅 (정적 파일)
// ============================================================================

static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = nullptr
};

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

// ============================================================================
// URI 라우팅 (API - Status)
// ============================================================================

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

// ============================================================================
// URI 라우팅 (API - Test)
// ============================================================================

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

// ============================================================================
// URI 라우팅 (API - Config)
// ============================================================================

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

static const httpd_uri_t uri_api_config_device_rf = {
    .uri = "/api/config/device/rf",
    .method = HTTP_POST,
    .handler = api_config_post_handler,
    .user_ctx = nullptr
};

// ============================================================================
// URI 라우팅 (API - LoRa)
// ============================================================================

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

// ============================================================================
// URI 라우팅 (API - Devices)
// ============================================================================

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
#endif // DEVICE_MODE_TX

// ============================================================================
// URI 라우팅 (API - License)
// ============================================================================

static const httpd_uri_t uri_api_license_validate = {
    .uri = "/api/license/validate",
    .method = HTTP_POST,
    .handler = api_license_validate_handler,
    .user_ctx = nullptr
};

// ============================================================================
// URI 라우팅 (API - Notices)
// ============================================================================

static const httpd_uri_t uri_api_notices = {
    .uri = "/api/notices",
    .method = HTTP_GET,
    .handler = api_notices_handler,
    .user_ctx = nullptr
};

// ============================================================================
// URI 라우팅 (API - LED)
// ============================================================================

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

// ============================================================================
// URI 라우팅 (CORS Preflight - OPTIONS)
// ============================================================================

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
    .uri = "/api/license/validate",
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

static const httpd_uri_t uri_options_api_led_colors = {
    .uri = "/api/led/colors",
    .method = HTTP_OPTIONS,
    .handler = options_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_options_api_brightness_broadcast = {
    .uri = "/api/brightness/broadcast",
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
    config.max_open_sockets = 10;
    config.max_uri_handlers = 48;
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

    // 정적 파일 등록
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_css);
    httpd_register_uri_handler(s_server, &uri_js);
    httpd_register_uri_handler(s_server, &uri_alpine);
    httpd_register_uri_handler(s_server, &uri_favicon);

    // API 등록
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

    httpd_register_uri_handler(s_server, &uri_api_lora_scan);
    httpd_register_uri_handler(s_server, &uri_api_lora_scan_start);
    httpd_register_uri_handler(s_server, &uri_api_lora_scan_stop);

    httpd_register_uri_handler(s_server, &uri_api_devices);
    httpd_register_uri_handler(s_server, &uri_api_delete_device);
    httpd_register_uri_handler(s_server, &uri_api_license_validate);
    httpd_register_uri_handler(s_server, &uri_api_test_internet);
    httpd_register_uri_handler(s_server, &uri_api_test_license_server);
    httpd_register_uri_handler(s_server, &uri_api_test_start);
    httpd_register_uri_handler(s_server, &uri_api_test_stop);
    httpd_register_uri_handler(s_server, &uri_api_notices);
    httpd_register_uri_handler(s_server, &uri_api_device_brightness);
    httpd_register_uri_handler(s_server, &uri_api_device_camera_id);
    httpd_register_uri_handler(s_server, &uri_api_led_colors_get);
    httpd_register_uri_handler(s_server, &uri_api_led_colors_post);

#ifdef DEVICE_MODE_TX
    httpd_register_uri_handler(s_server, &uri_api_brightness_broadcast);
    httpd_register_uri_handler(s_server, &uri_api_device_ping);
    httpd_register_uri_handler(s_server, &uri_api_device_stop);
    httpd_register_uri_handler(s_server, &uri_api_device_reboot);
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
