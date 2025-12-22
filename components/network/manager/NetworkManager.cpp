/**
 * @file NetworkManager.cpp
 * @brief 네트워크 통합 관리 Manager 구현 (TX 전용)
 */

// TX 모드에서만 빌드
#ifdef DEVICE_MODE_TX

#include "NetworkManager.h"
#include "ConfigCore.h"
#include "log.h"
#include "log_tags.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = TAG_NETWORK;

// 정적 멤버 초기화
bool NetworkManager::s_initialized = false;

esp_err_t NetworkManager::init()
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // 설정 로드
    const Config& config = ConfigCore::getAll();

    // WiFiCore 초기화 (AP+STA)
    const char* sta_ssid = config.wifi_sta.ssid[0] != '\0' ? config.wifi_sta.ssid : nullptr;
    const char* sta_password = config.wifi_sta.password[0] != '\0' ? config.wifi_sta.password : nullptr;

    esp_err_t err = WiFiCore::init(config.wifi_ap.ssid, config.wifi_ap.password,
                                     sta_ssid, sta_password);
    if (err != ESP_OK) {
        LOG_0(TAG, "WiFiCore 초기화 실패");
        return ESP_FAIL;
    }

    // EthernetCore 초기화 (W5500)
    err = EthernetCore::init(config.eth.dhcp_enabled,
                             config.eth.static_ip,
                             config.eth.static_netmask,
                             config.eth.static_gateway);
    if (err != ESP_OK) {
        LOG_0(TAG, "⚠ W5500 미장착");
        // 이더넷 실패는 치명적이지 않음 - 계속 진행
    } else {
        LOG_0(TAG, "✓ W5500 준비 완료");
    }

    s_initialized = true;

    return ESP_OK;
}

NetworkStatus NetworkManager::getStatus()
{
    NetworkStatus status = {};

    if (!s_initialized) {
        return status;
    }

    // WiFi 상태 가져오기
    WiFiStatus wifi_status = WiFiCore::getStatus();
    memcpy(&status.wifi_detail, &wifi_status, sizeof(WiFiStatus));

    // WiFi AP
    status.wifi_ap.active = true;
    status.wifi_ap.connected = wifi_status.ap_started;
    strncpy(status.wifi_ap.ip, wifi_status.ap_ip, sizeof(status.wifi_ap.ip) - 1);
    strncpy(status.wifi_ap.netmask, "255.255.255.0", sizeof(status.wifi_ap.netmask) - 1);
    strncpy(status.wifi_ap.gateway, wifi_status.ap_ip, sizeof(status.wifi_ap.gateway) - 1);

    // WiFi STA
    status.wifi_sta.active = true;
    status.wifi_sta.connected = wifi_status.sta_connected;
    if (wifi_status.sta_connected) {
        strncpy(status.wifi_sta.ip, wifi_status.sta_ip, sizeof(status.wifi_sta.ip) - 1);
    }

    // Ethernet 상태 가져오기
    EthernetStatus eth_status = EthernetCore::getStatus();
    memcpy(&status.eth_detail, &eth_status, sizeof(EthernetStatus));

    // Ethernet 인터페이스 상태
    status.ethernet.active = eth_status.initialized;
    status.ethernet.connected = eth_status.got_ip;
    if (eth_status.got_ip) {
        strncpy(status.ethernet.ip, eth_status.ip, sizeof(status.ethernet.ip) - 1);
        strncpy(status.ethernet.netmask, eth_status.netmask, sizeof(status.ethernet.netmask) - 1);
        strncpy(status.ethernet.gateway, eth_status.gateway, sizeof(status.ethernet.gateway) - 1);
    }

    return status;
}

void NetworkManager::printStatus()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return;
    }

    NetworkStatus status = getStatus();

    LOG_0(TAG, "--- 네트워크 상태 ---");

    // WiFi AP
    LOG_0(TAG, "[WiFi AP]");
    LOG_0(TAG, "  활성화: %s", status.wifi_ap.active ? "예" : "아니오");
    LOG_0(TAG, "  시작됨: %s", status.wifi_ap.connected ? "예" : "아니오");
    if (status.wifi_ap.connected) {
        LOG_0(TAG, "  IP: %s", status.wifi_ap.ip);
        LOG_0(TAG, "  클라이언트: %d명", status.wifi_detail.ap_clients);
    }

    // WiFi STA
    LOG_0(TAG, "[WiFi STA]");
    LOG_0(TAG, "  활성화: %s", status.wifi_sta.active ? "예" : "아니오");
    LOG_0(TAG, "  연결됨: %s", status.wifi_sta.connected ? "예" : "아니오");
    if (status.wifi_sta.connected) {
        LOG_0(TAG, "  IP: %s", status.wifi_sta.ip);
        LOG_0(TAG, "  신호 강도: %d dBm", status.wifi_detail.sta_rssi);
    }

    // Ethernet
    LOG_0(TAG, "[Ethernet (W5500)]");
    LOG_0(TAG, "  초기화: %s", status.ethernet.active ? "예" : "아니오");
    if (status.ethernet.active) {
        LOG_0(TAG, "  링크: %s", status.eth_detail.link_up ? "연결됨" : "연결 안됨");
        LOG_0(TAG, "  모드: %s", status.eth_detail.dhcp_mode ? "DHCP" : "Static");
        LOG_0(TAG, "  IP 할당: %s", status.ethernet.connected ? "예" : "아니오");
        if (status.ethernet.connected) {
            LOG_0(TAG, "  IP: %s", status.ethernet.ip);
            LOG_0(TAG, "  넷마스크: %s", status.ethernet.netmask);
            LOG_0(TAG, "  게이트웨이: %s", status.ethernet.gateway);
            LOG_0(TAG, "  MAC: %s", status.eth_detail.mac);
        }
    } else {
        LOG_0(TAG, "  (W5500 하드웨어 없음)");
    }

    LOG_0(TAG, "--------------------");
}

bool NetworkManager::isInitialized()
{
    return s_initialized;
}

esp_err_t NetworkManager::restartWiFi()
{
    LOG_0(TAG, "WiFi 재시작 중...");

    // WiFi 중지
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));

    // ConfigCore에서 새 설정 로드
    ConfigWiFiSTA wifi_sta = ConfigCore::getWiFiSTA();
    ConfigWiFiAP wifi_ap = ConfigCore::getWiFiAP();

    // WiFi AP 설정 업데이트
    wifi_config_t ap_config = {};
    ap_config.ap.ssid_len = strlen(wifi_ap.ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.pmf_cfg.required = false;
    strncpy((char*)ap_config.ap.ssid, wifi_ap.ssid, sizeof(ap_config.ap.ssid));
    strncpy((char*)ap_config.ap.password, wifi_ap.password, sizeof(ap_config.ap.password));
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    // WiFi STA 설정 업데이트
    if (wifi_sta.ssid[0] != '\0') {
        wifi_config_t sta_config = {};
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        sta_config.sta.pmf_cfg.capable = true;
        sta_config.sta.pmf_cfg.required = false;
        strncpy((char*)sta_config.sta.ssid, wifi_sta.ssid, sizeof(sta_config.sta.ssid));
        strncpy((char*)sta_config.sta.password, wifi_sta.password, sizeof(sta_config.sta.password));
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    }

    // WiFi 재시작
    esp_wifi_start();

    LOG_0(TAG, "WiFi 재시작 완료");
    return ESP_OK;
}

esp_err_t NetworkManager::restartEthernet()
{
    // Ethernet 설정 변경은 시스템 재시작이 필요합니다
    // EthernetCore::deinit() 메서드가 없어 안전한 재시작 불가
    LOG_0(TAG, "Ethernet 설정이 변경되었습니다. 재시작 필요");
    return ESP_OK;
}

#endif  // DEVICE_MODE_TX
