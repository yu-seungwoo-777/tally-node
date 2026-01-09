/**
 * @file WiFiDriver.cpp
 * @brief WiFi Driver 구현 (C++)
 */

#include "wifi_driver.h"
#include "wifi_hal.h"
#include "t_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include "lwip/dns.h"  // LwIP DNS (dns_setserver)

static const char* TAG = "04_WiFi";

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

    // 네트워크 상태 변경 콜백 타입
    using NetworkCallback = void (*)(bool connected, const char* ip);

    // 초기화 (AP+STA)
    static esp_err_t init(const char* ap_ssid, const char* ap_password,
        const char* sta_ssid, const char* sta_password);

    // 정리
    static esp_err_t deinit(void);

    // 상태 조회
    static Status getStatus(void);
    static bool isInitialized(void) { return s_initialized; }

    // STA 제어
    static esp_err_t reconnectSTA(void);
    static esp_err_t disconnectSTA(void);
    static esp_err_t reconfigSTA(const char* ssid, const char* password);
    static bool isSTAConnected(void) { return s_sta_connected; }

    // AP 제어
    static bool isAPStarted(void) { return s_ap_started; }
    static uint8_t getAPClients(void) { return s_ap_clients; }

    // 네트워크 상태 변경 콜백 설정
    static void setNetworkCallback(NetworkCallback callback) { s_network_callback = callback; }

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

    static NetworkCallback s_network_callback;
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

WiFiDriver::NetworkCallback WiFiDriver::s_network_callback = nullptr;
uint8_t WiFiDriver::s_sta_retry_count = 0;

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief WiFi 이벤트 핸들러
 * @param arg 사용자 인자 (미사용)
 * @param event_base 이벤트 베이스 (WIFI_EVENT 또는 IP_EVENT)
 * @param event_id 이벤트 ID
 * @param event_data 이벤트 데이터 포인터
 */
void WiFiDriver::eventHandler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START: {
                T_LOGI(TAG, "WiFi AP 시작됨");
                s_ap_started = true;
                // AP IP는 실제로는 192.168.4.1이지만 이벤트 시점에 아직 할당되지 않을 수 있음
                // getStatus()에서 실시간 조회로 처리
                break;
            }

            case WIFI_EVENT_AP_STOP: {
                T_LOGI(TAG, "WiFi AP 정지됨");
                s_ap_started = false;
                s_ap_clients = 0;
                memset(s_ap_ip, 0, sizeof(s_ap_ip));
                break;
            }

            case WIFI_EVENT_AP_STACONNECTED: {
                auto* event = (wifi_event_ap_staconnected_t*)event_data;
                T_LOGI(TAG, "STA 연결됨: " MACSTR, MAC2STR(event->mac));
                s_ap_clients++;
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                auto* event = (wifi_event_ap_stadisconnected_t*)event_data;
                T_LOGI(TAG, "STA 연결 해제됨: " MACSTR, MAC2STR(event->mac));
                if (s_ap_clients > 0) {
                    s_ap_clients--;
                }
                break;
            }

            case WIFI_EVENT_STA_START: {
                T_LOGI(TAG, "WiFi STA 시작됨, 연결 시도...");
                s_sta_retry_count = 0;
                esp_wifi_connect();
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                auto* event = (wifi_event_sta_disconnected_t*)event_data;
                T_LOGW(TAG, "STA 연결 해제됨: 이유=%d", event->reason);
                s_sta_connected = false;
                s_sta_rssi = 0;
                memset(s_sta_ip, 0, sizeof(s_sta_ip));

                // 네트워크 상태 변경 콜백 호출 (연결 해제)
                if (s_network_callback) {
                    s_network_callback(false, nullptr);
                }

                if (s_sta_retry_count < MAX_STA_RETRY) {
                    s_sta_retry_count++;
                    T_LOGI(TAG, "STA 재연결 시도 (%d/%d)...",
                        s_sta_retry_count, MAX_STA_RETRY);
                    vTaskDelay(pdMS_TO_TICKS(1000 * s_sta_retry_count));
                    esp_wifi_connect();
                } else {
                    T_LOGE(TAG, "STA 재연결 실패 (최대 재시도 도달)");
                }
                break;
            }

            case WIFI_EVENT_SCAN_DONE: {
                T_LOGD(TAG, "WiFi 스캔 완료");
                break;
            }

            default: {
                break;
            }
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            auto* event = (ip_event_got_ip_t*)event_data;
            s_sta_connected = true;
            s_sta_retry_count = 0;

            snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR,
                IP2STR(&event->ip_info.ip));

            T_LOGI(TAG, "STA IP 획득: " IPSTR, IP2STR(&event->ip_info.ip));
            T_LOGI(TAG, "STA Netmask: " IPSTR,
                IP2STR(&event->ip_info.netmask));
            T_LOGI(TAG, "STA Gateway: " IPSTR, IP2STR(&event->ip_info.gw));

            // DNS 서버 명시적 설정 (Google DNS, Cloudflare) - LwIP 직접 사용
            ip_addr_t dns_primary, dns_backup;
            dns_primary.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
            dns_primary.type = IPADDR_TYPE_V4;
            dns_backup.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
            dns_backup.type = IPADDR_TYPE_V4;

            // LwIP DNS 서버 직접 설정
            dns_setserver(0, &dns_primary);   // DNS_INDEX 0 = Primary
            dns_setserver(1, &dns_backup);    // DNS_INDEX 1 = Backup

            T_LOGI(TAG, "DNS 서버 설정 (LwIP): 8.8.8.8 (Primary), 1.1.1.1 (Backup)");

            // DNS 설정 확인
            const ip_addr_t* dns_check = dns_getserver(0);
            T_LOGI(TAG, "DNS Main 확인 (LwIP): " IPSTR,
                IP2STR(&dns_check->u_addr.ip4));

            // 네트워크 상태 변경 콜백 호출 (연결 성공)
            if (s_network_callback) {
                s_network_callback(true, s_sta_ip);
            }
        }
    }
}

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief WiFi Driver 초기화 (AP+STA 모드)
 * @param ap_ssid AP SSID (비활성화 시 nullptr)
 * @param ap_password AP 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @param sta_ssid STA SSID (비활성화 시 nullptr)
 * @param sta_password STA 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @return ESP_OK 성공, 에러 코드 실패
 */
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
        strncpy((char*)ap_config.ap.ssid, s_ap_ssid,
            sizeof(ap_config.ap.ssid) - 1);

        if (s_ap_password[0] != '\0') {
            strncpy((char*)ap_config.ap.password, s_ap_password,
                sizeof(ap_config.ap.password) - 1);
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
            // AP netif 정리
            if (s_netif_ap) {
                esp_netif_destroy(s_netif_ap);
                s_netif_ap = nullptr;
            }
            return ESP_FAIL;
        }

        wifi_config_t sta_config = {};
        // 빈 비밀번호면 open network, 아니면 WPA2_PSK
        if (s_sta_password[0] != '\0') {
            sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            strncpy((char*)sta_config.sta.password, s_sta_password,
                sizeof(sta_config.sta.password) - 1);
        } else {
            sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }
        strncpy((char*)sta_config.sta.ssid, s_sta_ssid,
            sizeof(sta_config.sta.ssid) - 1);

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

/**
 * @brief WiFi Driver 정리
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음
 */
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

/**
 * @brief WiFi 상태 조회
 * @return Status 구조체 (AP/STA 상태, IP, RSSI, 클라이언트 수)
 */
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
        if (esp_netif_get_ip_info(s_netif_ap, &ip_info) == ESP_OK &&
            ip_info.ip.addr != 0) {
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
// STA 제어
// ============================================================================

/**
 * @brief STA 재연결 시도
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음 또는 STA 비활성화
 */
esp_err_t WiFiDriver::reconnectSTA(void)
{
    if (!s_initialized || !s_sta_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "STA 재연결 시도...");
    s_sta_retry_count = 0;
    return wifi_hal_connect();
}

/**
 * @brief STA 연결 해제
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음 또는 STA 비활성화
 */
esp_err_t WiFiDriver::disconnectSTA(void)
{
    if (!s_initialized || !s_sta_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "STA 연결 해제");
    return wifi_hal_disconnect();
}

/**
 * @brief STA 설정 재구성 및 재연결
 * @param ssid 새 SSID
 * @param password 새 비밀번호 (오픈 네트워크 시 nullptr)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음, ESP_ERR_INVALID_ARG SSID가 null
 */
esp_err_t WiFiDriver::reconfigSTA(const char* ssid, const char* password)
{
    if (!s_initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        T_LOGE(TAG, "SSID가 null");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGI(TAG, "STA 재설정: SSID=%s", ssid);

    // 새 SSID/Password 저장
    strncpy(s_sta_ssid, ssid, sizeof(s_sta_ssid) - 1);
    s_sta_ssid[sizeof(s_sta_ssid) - 1] = '\0';
    s_sta_enabled = true;

    if (password) {
        strncpy(s_sta_password, password, sizeof(s_sta_password) - 1);
        s_sta_password[sizeof(s_sta_password) - 1] = '\0';
    } else {
        s_sta_password[0] = '\0';
    }

    // STA 연결 해제
    wifi_hal_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 새 STA 설정 적용
    wifi_config_t sta_config = {};
    // 빈 비밀번호면 open network, 아니면 WPA2_PSK
    if (s_sta_password[0] != '\0') {
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)sta_config.sta.password, s_sta_password,
            sizeof(sta_config.sta.password) - 1);
    } else {
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    strncpy((char*)sta_config.sta.ssid, s_sta_ssid,
        sizeof(sta_config.sta.ssid) - 1);

    esp_err_t ret = wifi_hal_set_config(WIFI_IF_STA, &sta_config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "STA 설정 변경 실패");
        return ret;
    }

    // STA 재연결 (재시도 카운트 초기화)
    s_sta_retry_count = 0;
    ret = wifi_hal_connect();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "STA 연결 실패");
        return ret;
    }

    T_LOGI(TAG, "STA 재설정 완료");
    return ESP_OK;
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

/**
 * @brief WiFi Driver 초기화 (AP+STA 모드)
 * @param ap_ssid AP SSID (비활성화 시 nullptr)
 * @param ap_password AP 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @param sta_ssid STA SSID (비활성화 시 nullptr)
 * @param sta_password STA 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_driver_init(const char* ap_ssid, const char* ap_password,
    const char* sta_ssid, const char* sta_password)
{
    return WiFiDriver::init(ap_ssid, ap_password, sta_ssid, sta_password);
}

/**
 * @brief WiFi Driver 정리
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음
 */
esp_err_t wifi_driver_deinit(void)
{
    return WiFiDriver::deinit();
}

/**
 * @brief WiFi 상태 조회
 * @return wifi_driver_status_t 구조체 (AP/STA 상태, IP, RSSI, 클라이언트 수)
 */
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

/**
 * @brief WiFi Driver 초기화 상태 확인
 * @return true 초기화됨, false 초기화되지 않음
 */
bool wifi_driver_is_initialized(void)
{
    return WiFiDriver::isInitialized();
}

/**
 * @brief STA 재연결 시도
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음 또는 STA 비활성화
 */
esp_err_t wifi_driver_sta_reconnect(void)
{
    return WiFiDriver::reconnectSTA();
}

/**
 * @brief STA 연결 해제
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음 또는 STA 비활성화
 */
esp_err_t wifi_driver_sta_disconnect(void)
{
    return WiFiDriver::disconnectSTA();
}

/**
 * @brief STA 설정 재구성 및 재연결
 * @param ssid 새 SSID
 * @param password 새 비밀번호 (오픈 네트워크 시 nullptr)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음, ESP_ERR_INVALID_ARG SSID가 null
 */
esp_err_t wifi_driver_sta_reconfig(const char* ssid, const char* password)
{
    return WiFiDriver::reconfigSTA(ssid, password);
}

/**
 * @brief STA 연결 상태 확인
 * @return true 연결됨, false 연결되지 않음
 */
bool wifi_driver_sta_is_connected(void)
{
    return WiFiDriver::isSTAConnected();
}

/**
 * @brief AP 시작 상태 확인
 * @return true AP 시작됨, false AP 시작되지 않음
 */
bool wifi_driver_ap_is_started(void)
{
    return WiFiDriver::isAPStarted();
}

/**
 * @brief AP 연결된 클라이언트 수 조회
 * @return 연결된 클라이언트 수 (0~4)
 */
uint8_t wifi_driver_get_ap_clients(void)
{
    return WiFiDriver::getAPClients();
}

/**
 * @brief 네트워크 상태 변경 콜백 설정
 * @param callback 콜백 함수 포인터 (연결/해제 시 호출됨)
 */
void wifi_driver_set_status_callback(wifi_driver_status_callback_t callback)
{
    WiFiDriver::setNetworkCallback(callback);
}

} // extern "C"
