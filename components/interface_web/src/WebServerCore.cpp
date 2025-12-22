/**
 * @file WebServerCore.cpp
 * @brief HTTP 웹서버 코어 구현 (TX 전용)
 */

// TX 모드에서만 빌드
#ifdef DEVICE_MODE_TX

#include "log.h"
#include "log_tags.h"
#include "WebServerCore.h"
#include "ApiHandler.h"
#include "MonitorApi.h"  // system_monitor 컴포넌트
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = TAG_WEB;

// 웹 리소스 (web_resources.c에서 정의)
extern "C" {
    extern const char index_html[];
    extern const size_t index_html_len;
    extern const char style_css[];
    extern const size_t style_css_len;
    extern const char app_js[];
    extern const size_t app_js_len;
}

// 정적 멤버 초기화
httpd_handle_t WebServerCore::s_server = nullptr;

// ============================================================================
// 웹 리소스 핸들러
// ============================================================================

esp_err_t WebServerCore::indexHandler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, index_html, index_html_len);
    return ESP_OK;
}

esp_err_t WebServerCore::styleHandler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/css; charset=utf-8");
    httpd_resp_send(req, style_css, style_css_len);
    return ESP_OK;
}

esp_err_t WebServerCore::appJsHandler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    httpd_resp_send(req, app_js, app_js_len);
    return ESP_OK;
}

// ============================================================================
// 공개 메서드
// ============================================================================

esp_err_t WebServerCore::init()
{
    if (s_server != nullptr) {
    LOG_0(TAG, "웹서버가 이미 실행 중입니다.");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 20;  // LoRa API 추가로 인해 16 → 20 증가
    config.lru_purge_enable = true;
    config.stack_size = 6144;  // WiFi 스캔 JSON 응답을 위해 스택 증가 (기본값: 4096)

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
  LOG_0(TAG, "웹서버 시작 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 웹 리소스 핸들러
    static const httpd_uri_t uri_get_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = indexHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_get_style = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = styleHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_get_app_js = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = appJsHandler,
        .user_ctx = nullptr
    };

    // 모니터링 API
    static const httpd_uri_t uri_api_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = MonitorApi::statusHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_system_health = {
        .uri = "/api/system/health",
        .method = HTTP_GET,
        .handler = MonitorApi::healthHandler,
        .user_ctx = nullptr
    };

    // 설정 API
    static const httpd_uri_t uri_api_config = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = ApiHandler::configGetHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_wifi_scan = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = ApiHandler::wifiScanHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_wifi = {
        .uri = "/api/config/wifi",
        .method = HTTP_POST,
        .handler = ApiHandler::configWifiHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_eth = {
        .uri = "/api/config/eth",
        .method = HTTP_POST,
        .handler = ApiHandler::configEthHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_switchers = {
        .uri = "/api/config/switchers",
        .method = HTTP_GET,
        .handler = ApiHandler::configSwitchersGetHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_switcher = {
        .uri = "/api/config/switcher",
        .method = HTTP_POST,
        .handler = ApiHandler::configSwitcherSetHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_switcher_mapping = {
        .uri = "/api/config/switcher/mapping",
        .method = HTTP_POST,
        .handler = ApiHandler::configSwitcherMappingHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_restart = {
        .uri = "/api/restart",
        .method = HTTP_POST,
        .handler = ApiHandler::restartHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_switcher_restart = {
        .uri = "/api/switcher/restart",
        .method = HTTP_POST,
        .handler = ApiHandler::switcherRestartHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_config_mode = {
        .uri = "/api/config/mode",
        .method = HTTP_POST,
        .handler = ApiHandler::configModeHandler,
        .user_ctx = nullptr
    };

    // LoRa API
    static const httpd_uri_t uri_api_lora_scan = {
        .uri = "/api/lora/scan",
        .method = HTTP_GET,
        .handler = ApiHandler::loraScanHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_lora_config = {
        .uri = "/api/lora/config",
        .method = HTTP_POST,
        .handler = ApiHandler::loraConfigHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_lora_status = {
        .uri = "/api/lora/status",
        .method = HTTP_GET,
        .handler = ApiHandler::loraStatusHandler,
        .user_ctx = nullptr
    };

    static const httpd_uri_t uri_api_lora_apply = {
        .uri = "/api/lora/apply",
        .method = HTTP_POST,
        .handler = ApiHandler::loraApplyHandler,
        .user_ctx = nullptr
    };

    // 핸들러 등록
    httpd_register_uri_handler(s_server, &uri_get_index);
    httpd_register_uri_handler(s_server, &uri_get_style);
    httpd_register_uri_handler(s_server, &uri_get_app_js);
    httpd_register_uri_handler(s_server, &uri_api_status);
    httpd_register_uri_handler(s_server, &uri_api_system_health);
    httpd_register_uri_handler(s_server, &uri_api_config);
    httpd_register_uri_handler(s_server, &uri_api_wifi_scan);
    httpd_register_uri_handler(s_server, &uri_api_config_wifi);
    httpd_register_uri_handler(s_server, &uri_api_config_eth);
    httpd_register_uri_handler(s_server, &uri_api_config_switchers);
    httpd_register_uri_handler(s_server, &uri_api_config_switcher);
    httpd_register_uri_handler(s_server, &uri_api_config_switcher_mapping);
    httpd_register_uri_handler(s_server, &uri_api_config_mode);
    httpd_register_uri_handler(s_server, &uri_api_restart);
    httpd_register_uri_handler(s_server, &uri_api_switcher_restart);
    httpd_register_uri_handler(s_server, &uri_api_lora_scan);
    httpd_register_uri_handler(s_server, &uri_api_lora_config);
    httpd_register_uri_handler(s_server, &uri_api_lora_status);
    httpd_register_uri_handler(s_server, &uri_api_lora_apply);

    return ESP_OK;
}

esp_err_t WebServerCore::stop()
{
    if (s_server == nullptr) {
        return ESP_OK;
    }

  LOG_0(TAG, "웹서버 중지...");

    esp_err_t ret = httpd_stop(s_server);
    s_server = nullptr;
    return ret;
}

esp_err_t WebServerCore::restart()
{
    stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    return init();
}

bool WebServerCore::isRunning()
{
    return s_server != nullptr;
}

#endif  // DEVICE_MODE_TX
