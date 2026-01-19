/**
 * @file NetworkService.cpp
 * @brief 네트워크 통합 관리 서비스 구현 (C++)
 */

#include "network_service.h"
#include "wifi_driver.h"
#include "ethernet_driver.h"
#include "t_log.h"
#include "event_bus.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "error_macros.h"
#include <cstring>

static const char* TAG = "03_Network";

// ============================================================================
// NetworkService 클래스 (싱글톤)
// ============================================================================

class NetworkServiceClass {
public:
    // 초기화/정리
    static esp_err_t init(void);  // 이벤트 기반 초기화 (EVT_CONFIG_DATA_CHANGED 대기)
    static esp_err_t initWithConfig(const app_network_config_t* config);
    static esp_err_t deinit(void);

    // 시작/정지 (태스크 제어)
    static esp_err_t start(void);
    static esp_err_t stop(void);

    // 상태 조회
    static network_status_t getStatus(void);
    static void printStatus(void);
    static bool isInitialized(void) { return s_initialized; }

    // 상태 이벤트 발행
    static void publishStatus(void);

    // 설정 업데이트
    static esp_err_t updateConfig(const app_network_config_t* config);

    // 재시작
    static esp_err_t restartWiFi(void);
    static esp_err_t reconnectWiFiSTA(const char* ssid, const char* password);
    static esp_err_t restartEthernet(void);
    static esp_err_t restartAll(void);

private:
    NetworkServiceClass() = delete;
    ~NetworkServiceClass() = delete;

    // 이벤트 핸들러
    static esp_err_t onRestartRequest(const event_data_t* event);
    static esp_err_t onConfigDataEvent(const event_data_t* event);
    static esp_err_t onBatteryEmptyChanged(const event_data_t* event);

    // 드라이버 콜백 핸들러
    static void onEthernetStatusChange(bool connected, const char* ip);
    static void onWiFiStatusChange(bool connected, const char* ip);

    // 태스크 함수
    static void status_publish_task(void* arg);

    // 정적 멤버
    static bool s_initialized;       // 서비스 초기화 상태 (이벤트 구독 완료)
    static bool s_driver_initialized; // 드라이버 초기화 상태 (WiFi/Ethernet init 완료)
    static bool s_running;
    static TaskHandle_t s_task_handle;
    static app_network_config_t s_config;
    static network_status_t s_last_status;  // 이전 상태 (변경 감지용)

    // 태스크 설정 상수
    static constexpr uint32_t STATUS_PUBLISH_INTERVAL_MS = 1000;  // 1초마다 상태 발행
    static constexpr uint32_t TASK_STACK_SIZE = 6144;  // 6KB (T_LOG 256바이트 스택 버퍼 사용 대응)
    static constexpr uint32_t TASK_PRIORITY = 4;
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool NetworkServiceClass::s_initialized = false;
bool NetworkServiceClass::s_driver_initialized = false;
bool NetworkServiceClass::s_running = false;
TaskHandle_t NetworkServiceClass::s_task_handle = nullptr;
app_network_config_t NetworkServiceClass::s_config = {};
network_status_t NetworkServiceClass::s_last_status = {};

// ============================================================================
// 초기화/정리
// ============================================================================

esp_err_t NetworkServiceClass::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "already initialized");
        return ESP_OK;
    }

    T_LOGI(TAG, "Network Service init (event-based, waiting for EVT_CONFIG_DATA_CHANGED)");

    // 이벤트 버스 구독 (재시작 요청, 설정 데이터 변경)
    event_bus_subscribe(EVT_NETWORK_RESTART_REQUEST, onRestartRequest);
    event_bus_subscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);

    // 드라이버 초기화는 EVT_CONFIG_DATA_CHANGED 이벤트 수신 후 수행
    T_LOGD(TAG, "event bus subscribed, waiting for config event");

    s_initialized = true;

    return ESP_OK;
}

esp_err_t NetworkServiceClass::initWithConfig(const app_network_config_t* config)
{
    RETURN_ERR_IF_NULL(config);

    if (s_initialized) {
        T_LOGW(TAG, "already initialized, updating config");
        // 이미 초기화된 경우 설정만 업데이트
        memcpy(&s_config, config, sizeof(app_network_config_t));
        return ESP_OK;
    }

    T_LOGI(TAG, "initializing...");

    // 설정 저장
    memcpy(&s_config, config, sizeof(app_network_config_t));

    // WiFi Driver 초기화
    esp_err_t ret = ESP_OK;
    if (s_config.wifi_ap.enabled || s_config.wifi_sta.enabled) {
        const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
        const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
        const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
        const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

        ret = wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "WiFi Driver init failed");
            return ret;
        }
        // 네트워크 상태 변경 콜백 등록
        wifi_driver_set_status_callback(onWiFiStatusChange);
    }

    // Ethernet Driver 초기화
    if (s_config.ethernet.enabled) {
        ret = ethernet_driver_init(
            s_config.ethernet.dhcp_enabled,
            s_config.ethernet.static_ip,
            s_config.ethernet.static_netmask,
            s_config.ethernet.static_gateway
        );
        if (ret != ESP_OK) {
            T_LOGW(TAG, "Ethernet Driver init failed (hardware may not be attached)");
            // Ethernet은 실패해도 계속 진행
        } else {
            // 네트워크 상태 변경 콜백 등록
            ethernet_driver_set_status_callback(onEthernetStatusChange);
        }
    }

    s_driver_initialized = true;

    // 이벤트 버스 구독은 init()에서 처리됨 (init 호출)
    esp_err_t init_ret = init();
    if (init_ret != ESP_OK) {
        T_LOGE(TAG, "init() failed");
        return init_ret;
    }

    T_LOGI(TAG, "init complete");

    // 초기화 완료 후 상태 발행 태스크 자동 시작
    start();

    return ESP_OK;
}

esp_err_t NetworkServiceClass::deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Network Service cleanup...");

    // 태스크 정지
    stop();

    // 이벤트 버스 구독 해제
    event_bus_unsubscribe(EVT_NETWORK_RESTART_REQUEST, onRestartRequest);
    event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);

    wifi_driver_deinit();
    ethernet_driver_deinit();

    s_initialized = false;
    s_driver_initialized = false;

    T_LOGI(TAG, "Network Service cleanup complete");
    return ESP_OK;
}

// ============================================================================
// 태스크 관련
// ============================================================================

void NetworkServiceClass::status_publish_task(void* arg)
{
    (void)arg;
    T_LOGI(TAG, "Network status publish task start");

    while (s_running) {
        // 1초마다 상태 이벤트 발행
        if (s_initialized) {
            publishStatus();
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_PUBLISH_INTERVAL_MS));
    }

    T_LOGI(TAG, "Network status publish task end");
    vTaskDelete(nullptr);
}

esp_err_t NetworkServiceClass::start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        T_LOGW(TAG, "already running");
        return ESP_OK;
    }

    s_running = true;

    BaseType_t ret = xTaskCreate(
        status_publish_task,
        "network_status_task",
        TASK_STACK_SIZE,
        nullptr,
        TASK_PRIORITY,
        &s_task_handle
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "task creation failed");
        s_running = false;
        return ESP_FAIL;
    }

    T_LOGI(TAG, "Network Service started (status publish task running)");
    return ESP_OK;
}

esp_err_t NetworkServiceClass::stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }

    T_LOGI(TAG, "Network Service stopping...");
    s_running = false;

    // 태스크 종료 대기
    if (s_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(200));
        s_task_handle = nullptr;
    }

    T_LOGI(TAG, "Network Service stopped");
    return ESP_OK;
}

// ============================================================================
// 상태 조회
// ============================================================================

network_status_t NetworkServiceClass::getStatus(void)
{
    network_status_t status = {};

    if (!s_initialized) {
        return status;
    }

    // WiFi AP 상태
    if (wifi_driver_is_initialized()) {
        status.wifi_ap.active = s_config.wifi_ap.enabled;
        status.wifi_ap.connected = wifi_driver_ap_is_started();

        wifi_driver_status_t wifi_status = wifi_driver_get_status();
        strncpy(status.wifi_ap.ip, wifi_status.ap_ip, sizeof(status.wifi_ap.ip));
        strncpy(status.wifi_ap.netmask, "255.255.255.0", sizeof(status.wifi_ap.netmask));
        strncpy(status.wifi_ap.gateway, "192.168.4.1", sizeof(status.wifi_ap.gateway));
    }

    // WiFi STA 상태 (드라이버 초기화된 경우 항상 확인)
    if (wifi_driver_is_initialized()) {
        status.wifi_sta.active = s_config.wifi_sta.enabled;
        status.wifi_sta.connected = wifi_driver_sta_is_connected();

        wifi_driver_status_t wifi_status = wifi_driver_get_status();
        strncpy(status.wifi_sta.ip, wifi_status.sta_ip, sizeof(status.wifi_sta.ip));
        strncpy(status.wifi_sta.netmask, "255.255.255.0", sizeof(status.wifi_sta.netmask));
        strncpy(status.wifi_sta.gateway, "192.168.1.1", sizeof(status.wifi_sta.gateway));
    }

    // Ethernet 상태
    if (ethernet_driver_is_initialized()) {
        ethernet_driver_status_t eth_status = ethernet_driver_get_status();
        status.ethernet.active = eth_status.initialized;
        status.ethernet.detected = eth_status.detected;
        status.ethernet.connected = eth_status.link_up && eth_status.got_ip;
        strncpy(status.ethernet.ip, eth_status.ip, sizeof(status.ethernet.ip));
        strncpy(status.ethernet.netmask, eth_status.netmask, sizeof(status.ethernet.netmask));
        strncpy(status.ethernet.gateway, eth_status.gateway, sizeof(status.ethernet.gateway));
    }

    return status;
}

esp_err_t NetworkServiceClass::updateConfig(const app_network_config_t* config)
{
    RETURN_ERR_IF_NULL(config);

    memcpy(&s_config, config, sizeof(app_network_config_t));
    return ESP_OK;
}

void NetworkServiceClass::printStatus(void)
{
    if (!s_initialized) {
        T_LOGI(TAG, "not initialized");
        return;
    }

    network_status_t status = getStatus();

    T_LOGI(TAG, "===== Network Status =====");

    // WiFi AP
    if (status.wifi_ap.active) {
        T_LOGI(TAG, "WiFi AP: %s", status.wifi_ap.connected ? "started" : "stopped");
        if (status.wifi_ap.connected) {
            T_LOGI(TAG, "  IP: %s", status.wifi_ap.ip);
        }
    } else {
        T_LOGI(TAG, "WiFi AP: disabled");
    }

    // WiFi STA
    if (status.wifi_sta.active) {
        T_LOGI(TAG, "WiFi STA: %s", status.wifi_sta.connected ? "connected" : "not connected");
        if (status.wifi_sta.connected) {
            T_LOGI(TAG, "  IP: %s", status.wifi_sta.ip);
        }
    } else {
        T_LOGI(TAG, "WiFi STA: disabled");
    }

    // Ethernet
    if (status.ethernet.active) {
        T_LOGI(TAG, "Ethernet: %s", status.ethernet.connected ? "connected" : "not connected");
        if (status.ethernet.connected) {
            T_LOGI(TAG, "  IP: %s", status.ethernet.ip);
        }
    } else {
        T_LOGI(TAG, "Ethernet: disabled");
    }

    T_LOGI(TAG, "=========================");
}

// ============================================================================
// 상태 이벤트 발행
// ============================================================================

void NetworkServiceClass::publishStatus(void)
{
    if (!s_initialized) {
        return;
    }

    network_status_t status = getStatus();

    // 디버그: 상태 변경 원인 추적
    bool sta_changed = (s_last_status.wifi_sta.connected != status.wifi_sta.connected ||
                       strncmp(s_last_status.wifi_sta.ip, status.wifi_sta.ip, sizeof(s_last_status.wifi_sta.ip)) != 0);
    bool eth_changed = (s_last_status.ethernet.connected != status.ethernet.connected ||
                       strncmp(s_last_status.ethernet.ip, status.ethernet.ip, sizeof(s_last_status.ethernet.ip)) != 0);
    bool ap_changed = (strncmp(s_last_status.wifi_ap.ip, status.wifi_ap.ip, sizeof(s_last_status.wifi_ap.ip)) != 0);

    T_LOGD(TAG, "publishStatus: sta_changed=%d eth_changed=%d ap_changed=%d", sta_changed, eth_changed, ap_changed);
    T_LOGD(TAG, "  eth: conn=%d (was %d), ip=%s (was %s)",
            status.ethernet.connected, s_last_status.ethernet.connected,
            status.ethernet.ip, s_last_status.ethernet.ip);

    if (sta_changed || eth_changed || ap_changed) {
        // stack 변수 사용 (이벤트 버스가 복사)
        network_status_event_t event;
        memset(&event, 0, sizeof(event));

        // 이벤트 데이터 생성
        event.ap_enabled = s_config.wifi_ap.enabled;
        event.sta_connected = status.wifi_sta.connected;
        event.eth_connected = status.ethernet.connected;
        event.eth_detected = status.ethernet.detected;
        event.eth_dhcp = s_config.ethernet.dhcp_enabled;

        strncpy(event.ap_ssid, s_config.wifi_ap.ssid, sizeof(event.ap_ssid) - 1);
        event.ap_ssid[sizeof(event.ap_ssid) - 1] = '\0';
        strncpy(event.ap_ip, status.wifi_ap.ip, sizeof(event.ap_ip) - 1);
        event.ap_ip[sizeof(event.ap_ip) - 1] = '\0';
        strncpy(event.sta_ssid, s_config.wifi_sta.ssid, sizeof(event.sta_ssid) - 1);
        event.sta_ssid[sizeof(event.sta_ssid) - 1] = '\0';
        strncpy(event.sta_ip, status.wifi_sta.ip, sizeof(event.sta_ip) - 1);
        event.sta_ip[sizeof(event.sta_ip) - 1] = '\0';
        strncpy(event.eth_ip, status.ethernet.ip, sizeof(event.eth_ip) - 1);
        event.eth_ip[sizeof(event.eth_ip) - 1] = '\0';

        event_bus_publish(EVT_NETWORK_STATUS_CHANGED, &event, sizeof(event));

        // 현재 상태 저장
        s_last_status = status;
    }
}

// ============================================================================
// 재시작
// ============================================================================

esp_err_t NetworkServiceClass::restartWiFi(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi restarting...");
    T_LOGI(TAG, "  wifi_sta.enabled=%d, wifi_ap.enabled=%d",
            s_config.wifi_sta.enabled, s_config.wifi_ap.enabled);

    // WiFi Driver가 초기화된 경우
    if (wifi_driver_is_initialized()) {
        // 완전 재초기화(deinit+init) 대신 stop+start 재설정 사용
        // 이유: esp_wifi_deinit()은 driver 구조체를 파괴하여 netif 참조 무효화 문제 발생

        // 새 설정 준비
        const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
        const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
        const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
        const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

        T_LOGI(TAG, "  sta_ssid=%p, sta_pass=%p", (void*)sta_ssid, (void*)sta_pass);

        // WiFi Driver 재설정 (stop+start)
        esp_err_t ret = wifi_driver_reconfigure(ap_ssid, ap_pass, sta_ssid, sta_pass);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "WiFi reset failed: %s", esp_err_to_name(ret));
            // 재설정 실패 시 완전 재초기화 시도
            wifi_driver_deinit();
            vTaskDelay(pdMS_TO_TICKS(100));
            ret = wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
        }
        return ret;
    }

    // WiFi Driver 미초기화 상태: 일반 초기화
    const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
    const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
    const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
    const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

    T_LOGI(TAG, "  sta_ssid=%p, sta_pass=%p", (void*)sta_ssid, (void*)sta_pass);

    return wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
}

esp_err_t NetworkServiceClass::reconnectWiFiSTA(const char* ssid, const char* password)
{
    T_LOGI(TAG, "WiFi STA reconnecting (AP preserved)...");

    // WiFi Driver가 초기화되지 않은 경우 먼저 초기화
    if (!wifi_driver_is_initialized()) {
        T_LOGI(TAG, "WiFi Driver not initialized, initializing");

        // AP 설정 유지 (AP가 활성화된 경우)
        const char* ap_ssid = (s_config.wifi_ap.enabled && s_config.wifi_ap.ssid[0] != '\0')
                             ? s_config.wifi_ap.ssid : nullptr;
        const char* ap_pass = (s_config.wifi_ap.enabled)
                             ? s_config.wifi_ap.password : nullptr;
        // STA 설정 (파라미터 우선)
        const char* sta_ssid = ssid;
        const char* sta_pass = password;

        esp_err_t ret = wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "WiFi Driver init failed: %s", esp_err_to_name(ret));
            return ret;
        }
        // 네트워크 상태 변경 콜백 등록
        wifi_driver_set_status_callback(onWiFiStatusChange);

        T_LOGI(TAG, "WiFi Driver init complete (reconnect requested)");
        return ESP_OK;
    }

    // STA만 재설정/재연결
    esp_err_t ret = wifi_driver_sta_reconfig(ssid, password);
    if (ret == ESP_OK) {
        // 내부 설정도 업데이트
        if (ssid) {
            strncpy(s_config.wifi_sta.ssid, ssid, sizeof(s_config.wifi_sta.ssid) - 1);
            s_config.wifi_sta.ssid[sizeof(s_config.wifi_sta.ssid) - 1] = '\0';
            s_config.wifi_sta.enabled = true;
        }
        if (password) {
            strncpy(s_config.wifi_sta.password, password, sizeof(s_config.wifi_sta.password) - 1);
            s_config.wifi_sta.password[sizeof(s_config.wifi_sta.password) - 1] = '\0';
        }
    }

    return ret;
}

esp_err_t NetworkServiceClass::restartEthernet(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Ethernet 정리
    ethernet_driver_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 설정에 따라 재시작 또는 정지 유지
    if (!s_config.ethernet.enabled) {
        T_LOGI(TAG, "Ethernet disabled (not restarting)");
        // 상태 변경 이벤트 발행 (스위처 폴백용)
        publishStatus();
        return ESP_OK;
    }

    T_LOGI(TAG, "Ethernet restarting...");

    // Ethernet 재시작 (s_config 사용)
    esp_err_t ret = ethernet_driver_init(
        s_config.ethernet.dhcp_enabled,
        s_config.ethernet.static_ip,
        s_config.ethernet.static_netmask,
        s_config.ethernet.static_gateway
    );

    if (ret == ESP_OK) {
        // 상태 변경 이벤트 발행
        publishStatus();
    }

    return ret;
}

esp_err_t NetworkServiceClass::restartAll(void)
{
    esp_err_t ret;

    ret = restartWiFi();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "WiFi restart failed");
    }

    ret = restartEthernet();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Ethernet restart failed");
    }

    return ESP_OK;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

esp_err_t NetworkServiceClass::onRestartRequest(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    auto* req = (network_restart_request_t*) event->data;

    switch (req->type) {
        case NETWORK_RESTART_WIFI_AP:
            T_LOGI(TAG, "event received: WiFi AP restart request");
            return restartWiFi();

        case NETWORK_RESTART_WIFI_STA:
            T_LOGI(TAG, "event received: WiFi STA reconnect request (AP preserved)");
            return reconnectWiFiSTA(req->ssid, req->password);

        case NETWORK_RESTART_ETHERNET:
            T_LOGI(TAG, "event received: Ethernet restart request");
            return restartEthernet();

        case NETWORK_RESTART_ALL:
            T_LOGI(TAG, "event received: full network restart request");
            {
                esp_err_t ret = restartAll();
                if (ret == ESP_OK) {
                    // 재시작 완료 후 이벤트 발행 (웹서버 재시작용)
                    event_bus_publish(EVT_NETWORK_RESTARTED, nullptr, 0);
                    T_LOGD(TAG, "network restart complete event published");
                }
                return ret;
            }

        default:
            T_LOGW(TAG, "unknown restart type: %d", req->type);
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t NetworkServiceClass::onConfigDataEvent(const event_data_t* event)
{
    RETURN_ERR_IF_NULL(event);

    // 이벤트 버스가 복사한 데이터를 직접 참조
    const config_data_event_t* config_event = (const config_data_event_t*)event->data;

    // WiFi AP 설정 업데이트
    s_config.wifi_ap.enabled = config_event->wifi_ap_enabled;
    s_config.wifi_ap.channel = config_event->wifi_ap_channel;
    strncpy(s_config.wifi_ap.ssid, config_event->wifi_ap_ssid, sizeof(s_config.wifi_ap.ssid) - 1);
    s_config.wifi_ap.ssid[sizeof(s_config.wifi_ap.ssid) - 1] = '\0';
    strncpy(s_config.wifi_ap.password, config_event->wifi_ap_password, sizeof(s_config.wifi_ap.password) - 1);
    s_config.wifi_ap.password[sizeof(s_config.wifi_ap.password) - 1] = '\0';

    // WiFi STA 설정 업데이트
    s_config.wifi_sta.enabled = config_event->wifi_sta_enabled;
    strncpy(s_config.wifi_sta.ssid, config_event->wifi_sta_ssid, sizeof(s_config.wifi_sta.ssid) - 1);
    s_config.wifi_sta.ssid[sizeof(s_config.wifi_sta.ssid) - 1] = '\0';
    strncpy(s_config.wifi_sta.password, config_event->wifi_sta_password, sizeof(s_config.wifi_sta.password) - 1);
    s_config.wifi_sta.password[sizeof(s_config.wifi_sta.password) - 1] = '\0';

    // Ethernet 설정 업데이트
    s_config.ethernet.dhcp_enabled = config_event->eth_dhcp_enabled;
    strncpy(s_config.ethernet.static_ip, config_event->eth_static_ip, sizeof(s_config.ethernet.static_ip) - 1);
    s_config.ethernet.static_ip[sizeof(s_config.ethernet.static_ip) - 1] = '\0';
    strncpy(s_config.ethernet.static_netmask, config_event->eth_static_netmask, sizeof(s_config.ethernet.static_netmask) - 1);
    s_config.ethernet.static_netmask[sizeof(s_config.ethernet.static_netmask) - 1] = '\0';
    strncpy(s_config.ethernet.static_gateway, config_event->eth_static_gateway, sizeof(s_config.ethernet.static_gateway) - 1);
    s_config.ethernet.static_gateway[sizeof(s_config.ethernet.static_gateway) - 1] = '\0';
    s_config.ethernet.enabled = config_event->eth_enabled;

    T_LOGI(TAG, "config data updated (event)");

    // 드라이버가 초기화되지 않았으면 드라이버 초기화 수행 (이벤트 기반 초기화)
    if (!s_driver_initialized) {
        T_LOGI(TAG, "driver init (event-based)");

        // WiFi Driver 초기화
        if (s_config.wifi_ap.enabled || s_config.wifi_sta.enabled) {
            const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
            const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
            const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
            const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

            esp_err_t ret = wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
            if (ret != ESP_OK) {
                T_LOGE(TAG, "WiFi Driver init failed (event-based)");
                return ret;
            }
            // 네트워크 상태 변경 콜백 등록
            wifi_driver_set_status_callback(onWiFiStatusChange);
        }

        // Ethernet Driver 초기화
        if (s_config.ethernet.enabled) {
            esp_err_t ret = ethernet_driver_init(
                s_config.ethernet.dhcp_enabled,
                s_config.ethernet.static_ip,
                s_config.ethernet.static_netmask,
                s_config.ethernet.static_gateway
            );
            if (ret != ESP_OK) {
                T_LOGW(TAG, "Ethernet Driver init failed (event-based, hardware may not be attached)");
                // Ethernet은 실패해도 계속 진행
            } else {
                // 네트워크 상태 변경 콜백 등록
                ethernet_driver_set_status_callback(onEthernetStatusChange);
            }
        }

        s_driver_initialized = true;
        T_LOGI(TAG, "driver init complete (event-based)");

        // 초기화 완료 후 상태 발행 태스크 자동 시작
        start();
    }

    return ESP_OK;
}

// ============================================================================
// 드라이버 콜백 핸들러
// ============================================================================

void NetworkServiceClass::onEthernetStatusChange(bool connected, const char* ip)
{
    if (connected) {
        T_LOGI(TAG, "Ethernet connected: %s", ip ? ip : "unknown");
        // 이벤트 버스로 네트워크 연결 발행
        if (ip) {
            event_bus_publish(EVT_NETWORK_CONNECTED, ip, strlen(ip) + 1);
        }
    } else {
        T_LOGW(TAG, "Ethernet disconnected");
        // 이벤트 버스로 네트워크 해제 발행
        event_bus_publish(EVT_NETWORK_DISCONNECTED, nullptr, 0);
    }
    // 네트워크 상태 변경 이벤트 발행 (DisplayManager 갱신용)
    publishStatus();
}

void NetworkServiceClass::onWiFiStatusChange(bool connected, const char* ip)
{
    if (connected) {
        T_LOGI(TAG, "WiFi connected: %s", ip ? ip : "unknown");
        // 이벤트 버스로 네트워크 연결 발행
        if (ip) {
            event_bus_publish(EVT_NETWORK_CONNECTED, ip, strlen(ip) + 1);
        }
    } else {
        T_LOGW(TAG, "WiFi disconnected");
        // 이벤트 버스로 네트워크 해제 발행
        event_bus_publish(EVT_NETWORK_DISCONNECTED, nullptr, 0);
    }
    // 네트워크 상태 변경 이벤트 발행 (DisplayManager 갱신용)
    publishStatus();
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t network_service_init(void)
{
    return NetworkServiceClass::init();
}

esp_err_t network_service_init_with_config(const app_network_config_t* config)
{
    return NetworkServiceClass::initWithConfig(config);
}

esp_err_t network_service_deinit(void)
{
    return NetworkServiceClass::deinit();
}

esp_err_t network_service_start(void)
{
    return NetworkServiceClass::start();
}

esp_err_t network_service_stop(void)
{
    return NetworkServiceClass::stop();
}

network_status_t network_service_get_status(void)
{
    return NetworkServiceClass::getStatus();
}

void network_service_print_status(void)
{
    NetworkServiceClass::printStatus();
}

bool network_service_is_initialized(void)
{
    return NetworkServiceClass::isInitialized();
}

esp_err_t network_service_restart_wifi(void)
{
    return NetworkServiceClass::restartWiFi();
}

esp_err_t network_service_reconnect_wifi_sta(const char* ssid, const char* password)
{
    return NetworkServiceClass::reconnectWiFiSTA(ssid, password);
}

esp_err_t network_service_restart_ethernet(void)
{
    return NetworkServiceClass::restartEthernet();
}

esp_err_t network_service_restart_all(void)
{
    return NetworkServiceClass::restartAll();
}

void network_service_publish_status(void)
{
    NetworkServiceClass::publishStatus();
}

} // extern "C"
