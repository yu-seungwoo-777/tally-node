/**
 * @file network_test_app.cpp
 * @brief 네트워크 테스트 앱 구현
 *
 * 기능:
 * - WiFi AP/STA 제어
 * - Ethernet (W5500) 제어
 * - WiFi 스캔
 * - 상태 모니터링
 * - 설정 관리
 */

#include "network_test_app.h"
#include "NetworkService.h"
#include "ConfigService.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "NetworkTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t network_test_app_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "네트워크 테스트 앱 초기화");
    ESP_LOGI(TAG, "========================================");

    // ConfigService 초기화
    ESP_LOGI(TAG, "ConfigService 초기화 중...");
    esp_err_t ret = config_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ConfigService 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "ConfigService 초기화 완료");

    // NetworkService 초기화 (ConfigService에서 설정 로드)
    ESP_LOGI(TAG, "NetworkService 초기화 중...");
    ret = network_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NetworkService 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NetworkService 초기화 완료");

    ESP_LOGI(TAG, "✓ 네트워크 테스트 앱 초기화 완료");
    return ESP_OK;
}

esp_err_t network_test_app_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "네트워크 테스트 앱 시작 중...");

    // NetworkService는 이미 init()에서 시작됨
    // 여기서는 상태만 확인
    network_status_t status = network_service_get_status();

    ESP_LOGI(TAG, "--- 현재 네트워크 상태 ---");
    if (status.wifi_ap.active) {
        ESP_LOGI(TAG, "WiFi AP: %s (IP: %s)",
                 status.wifi_ap.connected ? "연결됨" : "대기 중",
                 status.wifi_ap.ip);
    } else {
        ESP_LOGI(TAG, "WiFi AP: 비활성");
    }

    if (status.wifi_sta.active) {
        ESP_LOGI(TAG, "WiFi STA: %s (IP: %s)",
                 status.wifi_sta.connected ? "연결됨" : "대기 중",
                 status.wifi_sta.ip);
    } else {
        ESP_LOGI(TAG, "WiFi STA: 비활성");
    }

    if (status.ethernet.active) {
        ESP_LOGI(TAG, "Ethernet: %s (IP: %s)",
                 status.ethernet.connected ? "연결됨" : "대기 중",
                 status.ethernet.ip);
    } else {
        ESP_LOGI(TAG, "Ethernet: 비활성");
    }
    ESP_LOGI(TAG, "------------------------");

    s_running = true;
    ESP_LOGI(TAG, "✓ 네트워크 테스트 앱 시작 완료");
    return ESP_OK;
}

void network_test_app_stop(void)
{
    if (!s_running) {
        return;
    }

    ESP_LOGI(TAG, "네트워크 테스트 앱 정지 중...");
    s_running = false;
    ESP_LOGI(TAG, "✓ 네트워크 테스트 앱 정지 완료");
}

void network_test_app_deinit(void)
{
    network_test_app_stop();

    ESP_LOGI(TAG, "네트워크 테스트 앱 해제 중...");
    network_service_deinit();
    ESP_LOGI(TAG, "✓ 네트워크 테스트 앱 해제 완료");
}

bool network_test_app_is_running(void)
{
    return s_running;
}

// ============================================================================
// WiFi 테스트 함수
// ============================================================================

esp_err_t network_test_app_wifi_scan(void)
{
    if (!s_running) {
        ESP_LOGW(TAG, "앱이 시작되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "WiFi 스캔 시작...");

    // WiFi 스캔은 WiFiDriver를 통해서 수행해야 함
    // 여기서는 NetworkService를 통해 간접 호출
    // TODO: WiFiDriver의 scan 기능을 NetworkService에 노출하거나
    //       직접 WiFiDriver를 호출할 수 있는 방법 제공

    ESP_LOGI(TAG, "WiFi 스캔 기능: NetworkService에 연결 필요");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t network_test_app_wifi_sta_reconnect(void)
{
    if (!s_running) {
        ESP_LOGW(TAG, "앱이 시작되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "WiFi STA 재연결 시도...");
    esp_err_t ret = network_service_restart_wifi();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi STA 재연결 요청 완료");
    } else {
        ESP_LOGE(TAG, "WiFi STA 재연결 실패: %s", esp_err_to_name(ret));
    }
    return ret;
}

// ============================================================================
// Ethernet 테스트 함수
// ============================================================================

esp_err_t network_test_app_ethernet_restart(void)
{
    if (!s_running) {
        ESP_LOGW(TAG, "앱이 시작되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Ethernet 재시작 중...");
    esp_err_t ret = network_service_restart_ethernet();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet 재시작 완료");
    } else {
        ESP_LOGE(TAG, "Ethernet 재시작 실패: %s", esp_err_to_name(ret));
    }
    return ret;
}

// ============================================================================
// 상태 출력 함수
// ============================================================================

void network_test_app_print_status(void)
{
    network_service_print_status();
}

void network_test_app_print_config(void)
{
    config_all_t config;

    esp_err_t ret = config_service_load_all(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "설정 로드 실패: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "네트워크 설정");
    ESP_LOGI(TAG, "========================================");

    // WiFi AP 설정
    ESP_LOGI(TAG, "[WiFi AP]");
    ESP_LOGI(TAG, "  활성: %s", config.wifi_ap.enabled ? "예" : "아니오");
    if (config.wifi_ap.enabled) {
        ESP_LOGI(TAG, "  SSID: %s", config.wifi_ap.ssid);
        if (strlen(config.wifi_ap.password) > 0) {
            ESP_LOGI(TAG, "  Password: ********");
        }
        ESP_LOGI(TAG, "  Channel: %d", config.wifi_ap.channel);
    }

    // WiFi STA 설정
    ESP_LOGI(TAG, "[WiFi STA]");
    ESP_LOGI(TAG, "  활성: %s", config.wifi_sta.enabled ? "예" : "아니오");
    if (config.wifi_sta.enabled) {
        ESP_LOGI(TAG, "  SSID: %s", config.wifi_sta.ssid);
        if (strlen(config.wifi_sta.password) > 0) {
            ESP_LOGI(TAG, "  Password: ********");
        }
    }

    // Ethernet 설정
    ESP_LOGI(TAG, "[Ethernet]");
    ESP_LOGI(TAG, "  활성: %s", config.ethernet.enabled ? "예" : "아니오");
    if (config.ethernet.enabled) {
        ESP_LOGI(TAG, "  DHCP: %s", config.ethernet.dhcp_enabled ? "예" : "아니오");
        if (!config.ethernet.dhcp_enabled) {
            ESP_LOGI(TAG, "  Static IP: %s", config.ethernet.static_ip);
            ESP_LOGI(TAG, "  Netmask: %s", config.ethernet.static_netmask);
            ESP_LOGI(TAG, "  Gateway: %s", config.ethernet.static_gateway);
        }
    }

    ESP_LOGI(TAG, "========================================");
}

} // extern "C"
