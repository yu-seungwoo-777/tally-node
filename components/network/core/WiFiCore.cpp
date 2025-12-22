/**
 * @file WiFiCore.cpp
 * @brief WiFi AP+STA 제어 Core 구현
 */

#include "WiFiCore.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"

static const char* TAG = TAG_WIFI;

// 정적 멤버 초기화
EventGroupHandle_t WiFiCore::s_event_group = nullptr;
esp_netif_t* WiFiCore::s_netif_ap = nullptr;
esp_netif_t* WiFiCore::s_netif_sta = nullptr;
bool WiFiCore::s_initialized = false;
bool WiFiCore::s_ap_started = false;
bool WiFiCore::s_sta_connected = false;
int WiFiCore::s_sta_retry_num = 0;
uint8_t WiFiCore::s_ap_clients = 0;

void WiFiCore::eventHandler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    /* AP 이벤트 */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        LOG_0(TAG, "WiFi AP 시작됨");
        s_ap_started = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        LOG_0(TAG, "WiFi AP 중지됨");
        s_ap_started = false;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        s_ap_clients++;
        LOG_0(TAG, "클라이언트 연결: MAC=" MACSTR " AID=%d (총 %d명)",
                 MAC2STR(event->mac), event->aid, s_ap_clients);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        if (s_ap_clients > 0) s_ap_clients--;
        LOG_0(TAG, "클라이언트 연결 해제: MAC=" MACSTR " AID=%d (남은 %d명)",
                 MAC2STR(event->mac), event->aid, s_ap_clients);
    }

    /* STA 이벤트 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        LOG_0(TAG, "WiFi STA 시작, 연결 시도...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        if (s_sta_retry_num < MAX_STA_RETRY) {
            esp_wifi_connect();
            s_sta_retry_num++;
            LOG_0(TAG, "WiFi STA 재연결 시도 (%d/%d)", s_sta_retry_num, MAX_STA_RETRY);
        } else {
            xEventGroupSetBits(s_event_group, STA_FAIL_BIT);
            LOG_0(TAG, "WiFi STA 연결 실패 (최대 재시도 초과)");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_retry_num = 0;
        s_sta_connected = true;
        xEventGroupSetBits(s_event_group, STA_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        LOG_0(TAG, "WiFi STA IP 손실");
        s_sta_connected = false;
    }

    /* 스캔 완료 */
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        LOG_0(TAG, "WiFi 스캔 완료");
        xEventGroupSetBits(s_event_group, SCAN_DONE_BIT);
    }
}

esp_err_t WiFiCore::init(const char* ap_ssid, const char* ap_password,
                        const char* sta_ssid, const char* sta_password)
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // 이벤트 그룹 생성
    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        LOG_0(TAG, "이벤트 그룹 생성 실패");
        return ESP_FAIL;
    }

    // netif 초기화
    static bool netif_initialized = false;
    if (!netif_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        netif_initialized = true;
    }

    // WiFi netif 생성
    s_netif_ap = esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    // WiFi 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 이벤트 핸들러 등록
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &eventHandler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &eventHandler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                                &eventHandler, nullptr));

    // AP 설정
    wifi_config_t wifi_config_ap = {};
    wifi_config_ap.ap.ssid_len = strlen(ap_ssid);
    wifi_config_ap.ap.channel = 1;
    wifi_config_ap.ap.max_connection = 4;
    wifi_config_ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_ap.ap.pmf_cfg.required = false;
    strncpy((char*)wifi_config_ap.ap.ssid, ap_ssid, sizeof(wifi_config_ap.ap.ssid));
    strncpy((char*)wifi_config_ap.ap.password, ap_password, sizeof(wifi_config_ap.ap.password));

    // STA 설정
    wifi_config_t wifi_config_sta = {};
    wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config_sta.sta.pmf_cfg.capable = true;
    wifi_config_sta.sta.pmf_cfg.required = false;

    if (sta_ssid && sta_ssid[0] != '\0') {
        strncpy((char*)wifi_config_sta.sta.ssid, sta_ssid, sizeof(wifi_config_sta.sta.ssid));
        if (sta_password) {
            strncpy((char*)wifi_config_sta.sta.password, sta_password, sizeof(wifi_config_sta.sta.password));
        }
    }

    // WiFi 모드 설정 (AP+STA)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));

    // STA 설정 (SSID가 있을 때만)
    if (sta_ssid && sta_ssid[0] != '\0') {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
    }

    // WiFi 시작
    ESP_ERROR_CHECK(esp_wifi_start());

    // 전력 절약 비활성화 (저지연)
    esp_wifi_set_ps(WIFI_PS_NONE);

    s_initialized = true;

    LOG_0(TAG, "AP: %s (채널 1)", ap_ssid);
    if (sta_ssid && sta_ssid[0] != '\0') {
        LOG_0(TAG, "STA: %s 연결 시도 중...", sta_ssid);
    } else {
        LOG_0(TAG, "STA: 비활성화");
    }

    return ESP_OK;
}

WiFiStatus WiFiCore::getStatus()
{
    WiFiStatus status = {};

    if (!s_initialized) {
        return status;
    }

    status.ap_started = s_ap_started;
    status.sta_connected = s_sta_connected;
    status.ap_clients = s_ap_clients;

    // AP IP 가져오기
    if (s_netif_ap) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif_ap, &ip_info) == ESP_OK) {
            snprintf(status.ap_ip, sizeof(status.ap_ip),
                    IPSTR, IP2STR(&ip_info.ip));
        }
    }

    // STA IP 가져오기
    if (s_netif_sta && s_sta_connected) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif_sta, &ip_info) == ESP_OK) {
            snprintf(status.sta_ip, sizeof(status.sta_ip),
                    IPSTR, IP2STR(&ip_info.ip));
        }

        // RSSI 가져오기
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status.sta_rssi = ap_info.rssi;
        }
    }

    return status;
}

esp_err_t WiFiCore::scanStart()
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    // 스캔 비트 클리어
    xEventGroupClearBits(s_event_group, SCAN_DONE_BIT);

    // 스캔 시작
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        LOG_0(TAG, "스캔 시작 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    LOG_0(TAG, "WiFi 스캔 시작");
    return ESP_OK;
}

esp_err_t WiFiCore::getScanResults(wifi_ap_record_t* out_ap_records,
                                  uint16_t max_records, uint16_t* out_count)
{
    if (!out_ap_records || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_FAIL;
    }

    // 스캔 완료 대기 (최대 10초)
    EventBits_t bits = xEventGroupWaitBits(s_event_group,
                                          SCAN_DONE_BIT,
                                          pdTRUE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(10000));

    if (!(bits & SCAN_DONE_BIT)) {
        LOG_0(TAG, "스캔 타임아웃");
        return ESP_ERR_TIMEOUT;
    }

    // 스캔 결과 가져오기
    uint16_t number = max_records;
    esp_err_t err = esp_wifi_scan_get_ap_records(&number, out_ap_records);
    if (err != ESP_OK) {
        LOG_0(TAG, "스캔 결과 가져오기 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    *out_count = number;
    LOG_0(TAG, "스캔 결과: %d개 AP 발견", number);

    return ESP_OK;
}

esp_err_t WiFiCore::scan(WiFiScanResult* out_results, uint16_t max_results, uint16_t* out_count)
{
    if (!out_results || !out_count || max_results == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 스캔 시작
    esp_err_t ret = scanStart();
    if (ret != ESP_OK) {
        return ret;
    }

    // 결과 가져오기
    wifi_ap_record_t ap_records[max_results];
    uint16_t ap_count = 0;

    ret = getScanResults(ap_records, max_results, &ap_count);
    if (ret != ESP_OK) {
        return ret;
    }

    // 결과 변환
    for (int i = 0; i < ap_count; i++) {
        strncpy(out_results[i].ssid, (char*)ap_records[i].ssid, sizeof(out_results[i].ssid) - 1);
        out_results[i].ssid[sizeof(out_results[i].ssid) - 1] = '\0';
        out_results[i].channel = ap_records[i].primary;
        out_results[i].rssi = ap_records[i].rssi;
        out_results[i].auth_mode = ap_records[i].authmode;
    }

    *out_count = ap_count;
    return ESP_OK;
}

esp_err_t WiFiCore::reconnectSTA()
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    LOG_0(TAG, "WiFi STA 재연결 시도...");
    s_sta_retry_num = 0;
    xEventGroupClearBits(s_event_group, STA_CONNECTED_BIT | STA_FAIL_BIT);

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        LOG_0(TAG, "재연결 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t WiFiCore::disconnectSTA()
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    LOG_0(TAG, "WiFi STA 연결 해제");
    s_sta_connected = false;

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        LOG_0(TAG, "연결 해제 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

uint8_t WiFiCore::getAPClients()
{
    return s_ap_clients;
}

bool WiFiCore::isSTAConnected()
{
    return s_sta_connected;
}
