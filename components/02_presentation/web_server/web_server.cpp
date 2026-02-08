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
#include "esp_heap_caps.h"
#include "t_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <sys/socket.h>
#include <netinet/tcp.h>

static const char* TAG = "02_WS";
static httpd_handle_t s_server = nullptr;

// ============================================================================
// C 인터페이스
// ============================================================================

extern "C" {

static bool s_initialized = false;

// ============================================================================
// cJSON PSRAM 할당 함수
// ============================================================================

static void* cjson_malloc_psram(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!ptr) {
        // Fallback to internal RAM
        ptr = malloc(size);
    }
    return ptr;
}

static void cjson_free_psram(void* ptr) {
    heap_caps_free(ptr);
}

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

    // cJSON을 PSRAM으로 리디렉션 (메모리 절약)
    cJSON_Hooks hooks;
    hooks.malloc_fn = cjson_malloc_psram;
    hooks.free_fn = cjson_free_psram;
    cJSON_InitHooks(&hooks);
    T_LOGI(TAG, "cJSON configured to use PSRAM");

    // 캐시 초기화
    web_server_cache_init();

    // 정적 파일 PSRAM 캐싱 (420KB)
    extern void web_server_static_cache_init(void);
    web_server_static_cache_init();

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
 * @brief 소켓 오픈 콜백 - TCP_NODELAY 설정
 */
static esp_err_t web_server_socket_open(httpd_handle_t hd, int sockfd) {
    // TCP_NODELAY 활성화 (Nagle 알고리즘 비활성화)
    // 웹서버 응답 속도 개선을 위한 설정
    int nodelay = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        T_LOGW(TAG, "socket:fail:setsockopt:nodelay (fd=%d)", sockfd);
    }
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
    config.max_open_sockets = 7;  // 단일 사용자 웹서버 (최대 21 가능: LWIP_MAX_SOCKETS 24-3)
    config.max_uri_handlers = g_route_count + 4;  // 여유분 확보
    config.stack_size = 12288;  // 스택 오버플로우 방지 (8KB → 12KB, cJSON 생성 시 여유)
    config.task_priority = 5;  // 3→5 (event_bus와 동일, CPU 시간 확보)
    config.lru_purge_enable = true;
    // Keep-Alive 설정 (단일 사용자 최적화)
    config.keep_alive_enable = true;
    config.keep_alive_idle = 30;     // 30초 동안 유휴시 연결 유지
    config.keep_alive_interval = 5;  // 5초마다 keep-alive 프로브
    config.keep_alive_count = 3;     // 3회 실패시 연결 종료
    // TCP_NODELAY 활성화 (웹서버 응답 속도 개선)
    config.open_fn = web_server_socket_open;

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
