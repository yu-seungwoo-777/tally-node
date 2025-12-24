/**
 * @file NetworkService.cpp
 * @brief 네트워크 통합 관리 서비스 구현 (C++)
 */

#include "NetworkService.h"
#include "ConfigService.h"
#include "WiFiDriver.h"
#include "EthernetDriver.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "NetworkService";

// ============================================================================
// NetworkService 클래스 (싱글톤)
// ============================================================================

class NetworkServiceClass {
public:
    // 초기화/정리
    static esp_err_t init(void);
    static esp_err_t deinit(void);

    // 상태 조회
    static network_status_t getStatus(void);
    static void printStatus(void);
    static bool isInitialized(void) { return s_initialized; }

    // 재시작
    static esp_err_t restartWiFi(void);
    static esp_err_t restartEthernet(void);
    static esp_err_t restartAll(void);

private:
    NetworkServiceClass() = delete;
    ~NetworkServiceClass() = delete;

    // 정적 멤버
    static bool s_initialized;

    static config_all_t s_config;
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool NetworkServiceClass::s_initialized = false;
config_all_t NetworkServiceClass::s_config = {};

// ============================================================================
// 초기화/정리
// ============================================================================

esp_err_t NetworkServiceClass::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "Network Service 초기화 중...");

    // ConfigService 초기화
    esp_err_t ret = config_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "ConfigService 초기화 실패");
        return ret;
    }

    // 설정 로드
    ret = config_service_load_all(&s_config);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "설정 로드 실패, 기본값 사용");
        config_service_load_defaults(&s_config);
    }

    // WiFi Driver 초기화
    if (s_config.wifi_ap.enabled || s_config.wifi_sta.enabled) {
        const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
        const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
        const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
        const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

        ret = wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "WiFi Driver 초기화 실패");
            return ret;
        }
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
            T_LOGW(TAG, "Ethernet Driver 초기화 실패 (하드웨어 미장착 가능성)");
            // Ethernet은 실패해도 계속 진행
        }
    }

    s_initialized = true;

    T_LOGI(TAG, "Network Service 초기화 완료");
    return ESP_OK;
}

esp_err_t NetworkServiceClass::deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Network Service 정리 중...");

    wifi_driver_deinit();
    ethernet_driver_deinit();

    s_initialized = false;

    T_LOGI(TAG, "Network Service 정리 완료");
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
    if (s_config.wifi_ap.enabled) {
        status.wifi_ap.active = true;
        status.wifi_ap.connected = wifi_driver_ap_is_started();

        wifi_driver_status_t wifi_status = wifi_driver_get_status();
        strncpy(status.wifi_ap.ip, wifi_status.ap_ip, sizeof(status.wifi_ap.ip));
        strncpy(status.wifi_ap.netmask, "255.255.255.0", sizeof(status.wifi_ap.netmask));
        strncpy(status.wifi_ap.gateway, "192.168.4.1", sizeof(status.wifi_ap.gateway));
    }

    // WiFi STA 상태
    if (s_config.wifi_sta.enabled) {
        status.wifi_sta.active = true;
        status.wifi_sta.connected = wifi_driver_sta_is_connected();

        wifi_driver_status_t wifi_status = wifi_driver_get_status();
        strncpy(status.wifi_sta.ip, wifi_status.sta_ip, sizeof(status.wifi_sta.ip));
        strncpy(status.wifi_sta.netmask, "255.255.255.0", sizeof(status.wifi_sta.netmask));
        strncpy(status.wifi_sta.gateway, "192.168.1.1", sizeof(status.wifi_sta.gateway));
    }

    // Ethernet 상태
    if (s_config.ethernet.enabled) {
        ethernet_driver_status_t eth_status = ethernet_driver_get_status();
        status.ethernet.active = eth_status.initialized;
        status.ethernet.connected = eth_status.link_up && eth_status.got_ip;
        strncpy(status.ethernet.ip, eth_status.ip, sizeof(status.ethernet.ip));
        strncpy(status.ethernet.netmask, eth_status.netmask, sizeof(status.ethernet.netmask));
        strncpy(status.ethernet.gateway, eth_status.gateway, sizeof(status.ethernet.gateway));
    }

    return status;
}

void NetworkServiceClass::printStatus(void)
{
    if (!s_initialized) {
        T_LOGI(TAG, "초기화 안됨");
        return;
    }

    network_status_t status = getStatus();

    T_LOGI(TAG, "===== Network Status =====");

    // WiFi AP
    if (status.wifi_ap.active) {
        T_LOGI(TAG, "WiFi AP: %s", status.wifi_ap.connected ? "시작됨" : "정지됨");
        if (status.wifi_ap.connected) {
            T_LOGI(TAG, "  IP: %s", status.wifi_ap.ip);
        }
    } else {
        T_LOGI(TAG, "WiFi AP: 비활성화");
    }

    // WiFi STA
    if (status.wifi_sta.active) {
        T_LOGI(TAG, "WiFi STA: %s", status.wifi_sta.connected ? "연결됨" : "연결 안됨");
        if (status.wifi_sta.connected) {
            T_LOGI(TAG, "  IP: %s", status.wifi_sta.ip);
        }
    } else {
        T_LOGI(TAG, "WiFi STA: 비활성화");
    }

    // Ethernet
    if (status.ethernet.active) {
        T_LOGI(TAG, "Ethernet: %s", status.ethernet.connected ? "연결됨" : "연결 안됨");
        if (status.ethernet.connected) {
            T_LOGI(TAG, "  IP: %s", status.ethernet.ip);
        }
    } else {
        T_LOGI(TAG, "Ethernet: 비활성화");
    }

    T_LOGI(TAG, "=========================");
}

// ============================================================================
// 재시작
// ============================================================================

esp_err_t NetworkServiceClass::restartWiFi(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi 재시작 중...");

    // WiFi 정리
    wifi_driver_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 설정 재로드
    config_service_load_all(&s_config);

    // WiFi 재시작
    const char* ap_ssid = s_config.wifi_ap.enabled ? s_config.wifi_ap.ssid : nullptr;
    const char* ap_pass = s_config.wifi_ap.enabled ? s_config.wifi_ap.password : nullptr;
    const char* sta_ssid = s_config.wifi_sta.enabled ? s_config.wifi_sta.ssid : nullptr;
    const char* sta_pass = s_config.wifi_sta.enabled ? s_config.wifi_sta.password : nullptr;

    return wifi_driver_init(ap_ssid, ap_pass, sta_ssid, sta_pass);
}

esp_err_t NetworkServiceClass::restartEthernet(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Ethernet 재시작 중...");

    // Ethernet 정리
    ethernet_driver_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 설정 재로드
    config_service_load_all(&s_config);

    // Ethernet 재시작
    return ethernet_driver_init(
        s_config.ethernet.dhcp_enabled,
        s_config.ethernet.static_ip,
        s_config.ethernet.static_netmask,
        s_config.ethernet.static_gateway
    );
}

esp_err_t NetworkServiceClass::restartAll(void)
{
    esp_err_t ret;

    ret = restartWiFi();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "WiFi 재시작 실패");
    }

    ret = restartEthernet();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Ethernet 재시작 실패");
    }

    return ESP_OK;
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t network_service_init(void)
{
    return NetworkServiceClass::init();
}

esp_err_t network_service_deinit(void)
{
    return NetworkServiceClass::deinit();
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

esp_err_t network_service_restart_ethernet(void)
{
    return NetworkServiceClass::restartEthernet();
}

esp_err_t network_service_restart_all(void)
{
    return NetworkServiceClass::restartAll();
}

} // extern "C"
