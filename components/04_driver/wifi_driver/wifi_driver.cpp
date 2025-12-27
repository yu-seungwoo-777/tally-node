/**
 * @file WiFiDriver.cpp
 * @brief WiFi Driver 구현 (C++)
 */

#include "wifi_driver.h"
#include "wifi_hal.h"
#include "event_bus.h"
#include "t_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "WiFiDriver";

// ============================================================================
// WiFiDriver 클래스 (싱글톤)
// ============================================================================

class WiFiDriver {
public:
    // 상태 구조체
    struct Status {
        bool ap_started = false;
        bool sta_connected = false;
        char ap_ip[16] = {0};
        char sta_ip[16] = {0};
        int8_t sta_rssi = 0;
        uint8_t ap_clients = 0;
    };

    // 스캔 결과
    struct ScanResult {
        char ssid[33];
        uint8_t channel;
        int8_t rssi;
        uint8_t auth_mode;
    };

    // 콜백 타입
    using StatusCallback = void (*)();

    // 초기화 (AP+STA)
    static esp_err_t init(const char* ap_ssid, const char* ap_password,
                          const char* sta_ssid, const char* sta_password);

    // 정리
    static esp_err_t deinit(void);

    // 상태 조회
    static Status getStatus(void);
    static bool isInitialized(void) { return s_initialized; }

    // 스캔
    static esp_err_t scan(ScanResult* results, uint16_t max_count, uint16_t* out_count);

    // STA 제어
    static esp_err_t reconnectSTA(void);
    static esp_err_t disconnectSTA(void);
    static bool isSTAConnected(void) { return s_sta_connected; }

    // AP 제어
    static bool isAPStarted(void) { return s_ap_started; }
    static uint8_t getAPClients(void) { return s_ap_clients; }

    // 콜백 설정
    static void setStatusCallback(StatusCallback callback) { s_status_callback = callback; }

private:
    WiFiDriver() = delete;
    ~WiFiDriver() = delete;

    static void eventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);

    // 정적 멤버
    static bool s_initialized;
    static bool s_ap_enabled;
    static bool s_sta_enabled;
    static char s_ap_ssid[33];
    static char s_ap_password[65];
    static char s_sta_ssid[33];
    static char s_sta_password[65];

    static esp_netif_t* s_netif_ap;
    static esp_netif_t* s_netif_sta;

    static bool s_ap_started;
    static bool s_sta_connected;
    static int8_t s_sta_rssi;
    static uint8_t s_ap_clients;
    static char s_ap_ip[16];
    static char s_sta_ip[16];

    static StatusCallback s_status_callback;
    static uint8_t s_sta_retry_count;
    static constexpr uint8_t MAX_STA_RETRY = 5;
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool WiFiDriver::s_initialized = false;
bool WiFiDriver::s_ap_enabled = false;
bool WiFiDriver::s_sta_enabled = false;
char WiFiDriver::s_ap_ssid[33] = {0};
char WiFiDriver::s_ap_password[65] = {0};
char WiFiDriver::s_sta_ssid[33] = {0};
char WiFiDriver::s_sta_password[65] = {0};

esp_netif_t* WiFiDriver::s_netif_ap = nullptr;
esp_netif_t* WiFiDriver::s_netif_sta = nullptr;

bool WiFiDriver::s_ap_started = false;
bool WiFiDriver::s_sta_connected = false;
int8_t WiFiDriver::s_sta_rssi = 0;
uint8_t WiFiDriver::s_ap_clients = 0;
char WiFiDriver::s_ap_ip[16] = {0};
char WiFiDriver::s_sta_ip[16] = {0};

WiFiDriver::StatusCallback WiFiDriver::s_status_callback = nullptr;
uint8_t WiFiDriver::s_sta_retry_count = 0;

// ============================================================================
// 이벤트 핸들러
// ============================================================================

void WiFiDriver::eventHandler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                T_LOGI(TAG, "WiFi AP 시작됨");
                s_ap_started = true;
                // AP IP는 실제로는 192.168.4.1이지만 이벤트 시점에 아직 할당되지 않을 수 있음
                // getStatus()에서 실시간 조회로 처리
                if (s_status_callback) {
                    s_status_callback();
                }
                break;

            case WIFI_EVENT_AP_STOP:
                T_LOGI(TAG, "WiFi AP 정지됨");
                s_ap_started = false;
                s_ap_clients = 0;
                memset(s_ap_ip, 0, sizeof(s_ap_ip));
                if (s_status_callback) {
                    s_status_callback();
                }
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                auto* event = (wifi_event_ap_staconnected_t*) event_data;
                T_LOGI(TAG, "STA 연결됨: " MACSTR, MAC2STR(event->mac));
                s_ap_clients++;
                if (s_status_callback) {
                    s_status_callback();
                }
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                auto* event = (wifi_event_ap_stadisconnected_t*) event_data;
                T_LOGI(TAG, "STA 연결 해제됨: " MACSTR, MAC2STR(event->mac));
                if (s_ap_clients > 0) {
                    s_ap_clients--;
                }
                if (s_status_callback) {
                    s_status_callback();
                }
                break;
            }

            case WIFI_EVENT_STA_START:
                T_LOGI(TAG, "WiFi STA 시작됨, 연결 시도...");
                s_sta_retry_count = 0;
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                auto* event = (wifi_event_sta_disconnected_t*) event_data;
                T_LOGW(TAG, "STA 연결 해제됨: 이유=%d", event->reason);
                s_sta_connected = false;
                s_sta_rssi = 0;
                memset(s_sta_ip, 0, sizeof(s_sta_ip));

                // 이벤트 버스로 네트워크 해제 발행
                event_bus_publish(EVT_NETWORK_DISCONNECTED, nullptr, 0);

                if (s_sta_retry_count < MAX_STA_RETRY) {
                    s_sta_retry_count++;
                    T_LOGI(TAG, "STA 재연결 시도 (%d/%d)...", s_sta_retry_count, MAX_STA_RETRY);
                    vTaskDelay(pdMS_TO_TICKS(1000 * s_sta_retry_count));
                    esp_wifi_connect();
                } else {
                    T_LOGE(TAG, "STA 재연결 실패 (최대 재시도 도달)");
                }

                if (s_status_callback) {
                    s_status_callback();
                }
                break;
            }

            case WIFI_EVENT_SCAN_DONE:
                T_LOGD(TAG, "WiFi 스캔 완료");
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            auto* event = (ip_event_got_ip_t*) event_data;
            T_LOGI(TAG, "STA IP 획득: " IPSTR, IP2STR(&event->ip_info.ip));
            s_sta_connected = true;
            s_sta_retry_count = 0;

            snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&event->ip_info.ip));

            // 이벤트 버스로 네트워크 연결 발행
            event_bus_publish(EVT_NETWORK_CONNECTED, s_sta_ip, sizeof(s_sta_ip));

            if (s_status_callback) {
                s_status_callback();
            }
        }
    }
}

// ============================================================================
// 초기화/정리
// ============================================================================

esp_err_t WiFiDriver::init(const char* ap_ssid, const char* ap_password,
                           const char* sta_ssid, const char* sta_password)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "WiFi Driver 초기화 중...");

    // 설정 저장
    if (ap_ssid) {
        strncpy(s_ap_ssid, ap_ssid, sizeof(s_ap_ssid) - 1);
        s_ap_enabled = true;
        if (ap_password) {
            strncpy(s_ap_password, ap_password, sizeof(s_ap_password) - 1);
        }
    }

    if (sta_ssid) {
        strncpy(s_sta_ssid, sta_ssid, sizeof(s_sta_ssid) - 1);
        s_sta_enabled = true;
        if (sta_password) {
            strncpy(s_sta_password, sta_password, sizeof(s_sta_password) - 1);
        }
    }

    // WiFi HAL 초기화
    esp_err_t ret = wifi_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "WiFi HAL 초기화 실패");
        return ret;
    }

    // 이벤트 핸들러 등록
    wifi_hal_register_event_handler(eventHandler);

    // WiFi 모드 설정
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (s_ap_enabled && s_sta_enabled) {
        mode = WIFI_MODE_APSTA;
    } else if (s_ap_enabled) {
        mode = WIFI_MODE_AP;
    } else if (s_sta_enabled) {
        mode = WIFI_MODE_STA;
    }
    esp_wifi_set_mode(mode);

    // AP netif 및 설정
    if (s_ap_enabled) {
        s_netif_ap = (esp_netif_t*)wifi_hal_create_ap_netif();
        if (!s_netif_ap) {
            T_LOGE(TAG, "AP netif 생성 실패");
            return ESP_FAIL;
        }

        wifi_config_t ap_config = {};
        ap_config.ap.ssid_len = strlen(s_ap_ssid);
        ap_config.ap.channel = 1;
        ap_config.ap.max_connection = 4;
        ap_config.ap.beacon_interval = 100;
        strncpy((char*)ap_config.ap.ssid, s_ap_ssid, sizeof(ap_config.ap.ssid) - 1);

        if (s_ap_password[0] != '\0') {
            strncpy((char*)ap_config.ap.password, s_ap_password, sizeof(ap_config.ap.password) - 1);
            ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            ap_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        wifi_hal_set_config(WIFI_IF_AP, &ap_config);
    }

    // STA netif 및 설정
    if (s_sta_enabled) {
        s_netif_sta = (esp_netif_t*)wifi_hal_create_sta_netif();
        if (!s_netif_sta) {
            T_LOGE(TAG, "STA netif 생성 실패");
            return ESP_FAIL;
        }

        wifi_config_t sta_config = {};
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)sta_config.sta.ssid, s_sta_ssid, sizeof(sta_config.sta.ssid) - 1);

        if (s_sta_password[0] != '\0') {
            strncpy((char*)sta_config.sta.password, s_sta_password, sizeof(sta_config.sta.password) - 1);
        }

        wifi_hal_set_config(WIFI_IF_STA, &sta_config);
    }

    // WiFi 시작
    ret = wifi_hal_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "WiFi 시작 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;

    T_LOGI(TAG, "WiFi Driver 초기화 완료");
    T_LOGI(TAG, "  AP: %s (%s)", s_ap_enabled ? s_ap_ssid : "비활성화",
             s_ap_password[0] != '\0' ? "보호됨" : "열림");
    T_LOGI(TAG, "  STA: %s (%s)", s_sta_enabled ? s_sta_ssid : "비활성화",
             s_sta_password[0] != '\0' ? "보호됨" : "열림");

    return ESP_OK;
}

esp_err_t WiFiDriver::deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi Driver 정리 중...");

    wifi_hal_stop();
    wifi_hal_deinit();

    s_initialized = false;
    s_ap_started = false;
    s_sta_connected = false;

    T_LOGI(TAG, "WiFi Driver 정리 완료");
    return ESP_OK;
}

// ============================================================================
// 상태 조회
// ============================================================================

WiFiDriver::Status WiFiDriver::getStatus(void)
{
    Status status;
    status.ap_started = s_ap_started;
    status.sta_connected = s_sta_connected;
    status.sta_rssi = s_sta_rssi;
    status.ap_clients = s_ap_clients;

    // STA IP (이벤트에서 설정된 값 사용)
    strncpy(status.sta_ip, s_sta_ip, sizeof(status.sta_ip));

    // AP IP: 실시간 조회 (이벤트 시점에 IP가 할당되지 않을 수 있음)
    if (s_ap_started && s_netif_ap) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif_ap, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            snprintf(s_ap_ip, sizeof(s_ap_ip), IPSTR, IP2STR(&ip_info.ip));
        }
    }
    strncpy(status.ap_ip, s_ap_ip, sizeof(status.ap_ip));

    // STA RSSI 갱신
    if (s_sta_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_sta_rssi = ap_info.rssi;
            status.sta_rssi = s_sta_rssi;
        }
    }

    return status;
}

// ============================================================================
// 스캔
// ============================================================================

esp_err_t WiFiDriver::scan(ScanResult* results, uint16_t max_count, uint16_t* out_count)
{
    if (!s_initialized || !s_sta_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wifi_hal_scan_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "스캔 시작 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 스캔 완료 대기
    int retry = 100;
    while (retry-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        if (ap_count > 0) {
            break;
        }
    }

    uint16_t actual_count = 0;
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*)calloc(max_count, sizeof(wifi_ap_record_t));
    if (!ap_records) {
        return ESP_ERR_NO_MEM;
    }

    ret = wifi_hal_scan_get_results(ap_records, max_count, &actual_count);

    if (ret == ESP_OK && out_count) {
        *out_count = (actual_count < max_count) ? actual_count : max_count;

        for (uint16_t i = 0; i < *out_count; i++) {
            strncpy(results[i].ssid, (char*)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
            results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
            results[i].channel = ap_records[i].primary;
            results[i].rssi = ap_records[i].rssi;
            results[i].auth_mode = ap_records[i].authmode;
        }
    }

    free(ap_records);
    return ret;
}

// ============================================================================
// STA 제어
// ============================================================================

esp_err_t WiFiDriver::reconnectSTA(void)
{
    if (!s_initialized || !s_sta_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "STA 재연결 시도...");
    s_sta_retry_count = 0;
    return wifi_hal_connect();
}

esp_err_t WiFiDriver::disconnectSTA(void)
{
    if (!s_initialized || !s_sta_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "STA 연결 해제");
    return wifi_hal_disconnect();
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t wifi_driver_init(const char* ap_ssid, const char* ap_password,
                           const char* sta_ssid, const char* sta_password)
{
    return WiFiDriver::init(ap_ssid, ap_password, sta_ssid, sta_password);
}

esp_err_t wifi_driver_deinit(void)
{
    return WiFiDriver::deinit();
}

wifi_driver_status_t wifi_driver_get_status(void)
{
    auto cpp_status = WiFiDriver::getStatus();
    wifi_driver_status_t c_status;
    c_status.ap_started = cpp_status.ap_started;
    c_status.sta_connected = cpp_status.sta_connected;
    c_status.sta_rssi = cpp_status.sta_rssi;
    c_status.ap_clients = cpp_status.ap_clients;
    strncpy(c_status.ap_ip, cpp_status.ap_ip, sizeof(c_status.ap_ip));
    strncpy(c_status.sta_ip, cpp_status.sta_ip, sizeof(c_status.sta_ip));
    return c_status;
}

bool wifi_driver_is_initialized(void)
{
    return WiFiDriver::isInitialized();
}

esp_err_t wifi_driver_scan(wifi_driver_scan_result_t* results,
                           uint16_t max_count,
                           uint16_t* out_count)
{
    return WiFiDriver::scan((WiFiDriver::ScanResult*)results, max_count, out_count);
}

esp_err_t wifi_driver_sta_reconnect(void)
{
    return WiFiDriver::reconnectSTA();
}

esp_err_t wifi_driver_sta_disconnect(void)
{
    return WiFiDriver::disconnectSTA();
}

bool wifi_driver_sta_is_connected(void)
{
    return WiFiDriver::isSTAConnected();
}

bool wifi_driver_ap_is_started(void)
{
    return WiFiDriver::isAPStarted();
}

uint8_t wifi_driver_get_ap_clients(void)
{
    return WiFiDriver::getAPClients();
}

void wifi_driver_set_status_callback(wifi_driver_status_callback_t callback)
{
    WiFiDriver::setStatusCallback(callback);
}

} // extern "C"
