/**
 * @file web_server_routes.cpp
 * @brief Web Server URI 라우팅 테이블 구현
 */

#include "web_server_routes.h"
#include "handlers/api_static.h"
#include "handlers/api_status.h"
#include "handlers/api_config.h"
#include "handlers/api_lora.h"
#include "handlers/api_devices.h"
#include "handlers/api_test.h"
#include "handlers/api_led.h"
#include "handlers/api_license.h"
#include "handlers/api_notices.h"
#include "web_server_helpers.h"

// ============================================================================
// URI 라우팅 테이블 (중앙 정의)
// ============================================================================

// C++에서 designated initializers를 사용하여 라우트 정의
extern "C" {

const httpd_uri_t g_routes[] = {
    // 정적 파일
    { .uri = "/",                   .method = HTTP_GET,    .handler = index_handler,        .user_ctx = nullptr },
    { .uri = "/css/styles.css",     .method = HTTP_GET,    .handler = css_handler,          .user_ctx = nullptr },
    { .uri = "/js/app.bundle.js",   .method = HTTP_GET,    .handler = js_handler,           .user_ctx = nullptr },
    { .uri = "/vendor/alpine.js",   .method = HTTP_GET,    .handler = alpine_handler,       .user_ctx = nullptr },
    { .uri = "/favicon.ico",        .method = HTTP_GET,    .handler = favicon_handler,      .user_ctx = nullptr },

    // API - Status
    { .uri = "/api/status",         .method = HTTP_GET,    .handler = api_status_handler,   .user_ctx = nullptr },
    { .uri = "/api/reboot",         .method = HTTP_POST,   .handler = api_reboot_handler,   .user_ctx = nullptr },
    { .uri = "/api/reboot/broadcast", .method = HTTP_POST, .handler = api_reboot_broadcast_handler, .user_ctx = nullptr },

    // API - Config
    { .uri = "/api/config/network/ap",      .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/network/wifi",    .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/network/ethernet", .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/switcher/primary",   .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/switcher/secondary", .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/switcher/dual",      .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },
    { .uri = "/api/config/device/rf",          .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = nullptr },

    // API - LoRa
    { .uri = "/api/lora/scan",       .method = HTTP_GET,    .handler = api_lora_scan_get_handler,   .user_ctx = nullptr },
    { .uri = "/api/lora/scan/start", .method = HTTP_POST,   .handler = api_lora_scan_start_handler, .user_ctx = nullptr },
    { .uri = "/api/lora/scan/stop",  .method = HTTP_POST,   .handler = api_lora_scan_stop_handler,  .user_ctx = nullptr },

    // API - Devices
    { .uri = "/api/devices",         .method = HTTP_GET,    .handler = api_devices_handler,    .user_ctx = nullptr },
    { .uri = "/api/devices",         .method = HTTP_DELETE, .handler = api_delete_device_handler, .user_ctx = nullptr },
    { .uri = "/api/device/brightness", .method = HTTP_POST, .handler = api_device_brightness_handler, .user_ctx = nullptr },
    { .uri = "/api/device/camera-id",  .method = HTTP_POST, .handler = api_device_camera_id_handler,  .user_ctx = nullptr },

    // API - License
    { .uri = "/api/license/validate", .method = HTTP_POST, .handler = api_license_validate_handler, .user_ctx = nullptr },

    // API - Test
    { .uri = "/api/test/internet",       .method = HTTP_POST, .handler = api_test_internet_handler, .user_ctx = nullptr },
    { .uri = "/api/test/license-server", .method = HTTP_POST, .handler = api_test_license_server_handler, .user_ctx = nullptr },
    { .uri = "/api/test/start",          .method = HTTP_POST, .handler = api_test_start_handler, .user_ctx = nullptr },
    { .uri = "/api/test/stop",           .method = HTTP_POST, .handler = api_test_stop_handler, .user_ctx = nullptr },

    // API - Notices
    { .uri = "/api/notices",         .method = HTTP_GET,    .handler = api_notices_handler,    .user_ctx = nullptr },

    // API - LED
    { .uri = "/api/led/colors",      .method = HTTP_GET,    .handler = api_led_colors_get_handler, .user_ctx = nullptr },
    { .uri = "/api/led/colors",      .method = HTTP_POST,   .handler = api_led_colors_post_handler, .user_ctx = nullptr },

    // TX 전용
    { .uri = "/api/brightness/broadcast", .method = HTTP_POST, .handler = api_brightness_broadcast_handler, .user_ctx = nullptr },
    { .uri = "/api/device/ping",          .method = HTTP_POST, .handler = api_device_ping_handler, .user_ctx = nullptr },
    { .uri = "/api/device/stop",          .method = HTTP_POST, .handler = api_device_stop_handler, .user_ctx = nullptr },
    { .uri = "/api/device/reboot",        .method = HTTP_POST, .handler = api_device_reboot_handler, .user_ctx = nullptr },
    { .uri = "/api/device/status-request", .method = HTTP_POST, .handler = api_status_request_handler, .user_ctx = nullptr },

    // CORS OPTIONS
    { .uri = "/api/status",         .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/reboot",         .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/reboot/broadcast", .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/config/*",       .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/lora/*",         .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/devices",        .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/license/validate", .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/test/*",         .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/notices",        .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/brightness", .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/camera-id",  .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/led/colors",     .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/brightness/broadcast", .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/ping",          .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/stop",          .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/reboot",        .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
    { .uri = "/api/device/status-request", .method = HTTP_OPTIONS, .handler = web_server_options_handler, .user_ctx = nullptr },
};

const size_t g_route_count = sizeof(g_routes) / sizeof(g_routes[0]);

} // extern "C"
