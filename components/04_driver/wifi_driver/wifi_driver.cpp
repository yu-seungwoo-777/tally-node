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

    // 재설정 (deinit+init 없이 stop+start만으로 재설정)
    static esp_err_t reconfigure(const char* ap_ssid, const char* ap_password,
        const char* sta_ssid, const char* sta_password);

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
    static bool s_sta_auth_failed;  // 인증 실패 상태 (비밀번호 오류 등)
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
bool WiFiDriver::s_sta_auth_failed = false;

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
                T_LOGI(TAG, "WIFI_EVENT: AP started");
                s_ap_started = true;
                // AP IP는 실제로는 192.168.4.1이지만 이벤트 시점에 아직 할당되지 않을 수 있음
                // getStatus()에서 실시간 조회로 처리
                break;
            }

            case WIFI_EVENT_AP_STOP: {
                T_LOGI(TAG, "WIFI_EVENT: AP stopped");
                s_ap_started = false;
                s_ap_clients = 0;
                memset(s_ap_ip, 0, sizeof(s_ap_ip));
                break;
            }

            case WIFI_EVENT_AP_STACONNECTED: {
                auto* event = (wifi_event_ap_staconnected_t*)event_data;
                T_LOGI(TAG, "WIFI_EVENT: STA connected: " MACSTR, MAC2STR(event->mac));
                s_ap_clients++;
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                auto* event = (wifi_event_ap_stadisconnected_t*)event_data;
                T_LOGI(TAG, "WIFI_EVENT: STA disconnected: " MACSTR, MAC2STR(event->mac));
                if (s_ap_clients > 0) {
                    s_ap_clients--;
                }
                break;
            }

            case WIFI_EVENT_STA_START: {
                T_LOGI(TAG, "WIFI_EVENT: STA started, connecting...");
                s_sta_retry_count = 0;
                esp_wifi_connect();
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                auto* event = (wifi_event_sta_disconnected_t*)event_data;
                T_LOGW(TAG, "WIFI_EVENT: STA disconnected: reason=%d", event->reason);
                s_sta_connected = false;
                s_sta_rssi = 0;
                memset(s_sta_ip, 0, sizeof(s_sta_ip));

                // 네트워크 상태 변경 콜백 호출 (연결 해제)
                if (s_network_callback) {
                    s_network_callback(false, nullptr);
                }

                // 인증 실패 경우 (비밀번호 오류 등) - 플래그 설정 후 재시도 중지
                // 15: 4WAY_HANDSHAKE_TIMEOUT, 202: AUTH_FAIL, 203: ASSOC_FAIL, 205: AUTH_EXPIRE
                if (event->reason == 15 || event->reason == 202 ||
                    event->reason == 203 || event->reason == 205) {
                    s_sta_auth_failed = true;
                    T_LOGE(TAG, "Authentication failed (reason=%d), retry stopped. Check settings.",
                           event->reason);
                    break;
                }

                // 인증 실패 상태면 재시도하지 않음
                if (s_sta_auth_failed) {
                    T_LOGW(TAG, "Authentication failed state, retry stopped");
                    break;
                }

                // 무한 재연결 (공유기 재부팅 등 대응)
                s_sta_retry_count++;
                // 지연 시간: 최대 5초로 제한 (1, 2, 3, 4, 5, 5, 5, ...)
                uint32_t delay_sec = (s_sta_retry_count > 5) ? 5 : s_sta_retry_count;
                T_LOGI(TAG, "STA reconnecting (%d)... delay %d sec",
                        s_sta_retry_count, delay_sec);
                vTaskDelay(pdMS_TO_TICKS(1000 * delay_sec));
                esp_wifi_connect();
                break;
            }

            case WIFI_EVENT_SCAN_DONE: {
                T_LOGD(TAG, "WIFI_EVENT: Scan done");
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
            s_sta_auth_failed = false;  // 인증 성공, 플래그 리셋

            snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR,
                IP2STR(&event->ip_info.ip));

            T_LOGI(TAG, "IP_EVENT: STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            T_LOGD(TAG, "  Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));
            T_LOGD(TAG, "  Gateway: " IPSTR, IP2STR(&event->ip_info.gw));

            // DNS 서버 명시적 설정 (Google DNS, Cloudflare)
            // ESP-IDF 5.5.0: esp_netif_set_dns_info 사용
            esp_netif_dns_info_t dns_info;
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
            esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_MAIN, &dns_info);

            dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
            esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_BACKUP, &dns_info);

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
    T_LOGD(TAG, "Initializing WiFi Driver");

    // 설정 저장
    s_ap_enabled = (ap_ssid != nullptr);
    s_sta_enabled = (sta_ssid != nullptr);

    if (ap_ssid) {
        strncpy(s_ap_ssid, ap_ssid, sizeof(s_ap_ssid) - 1);
        if (ap_password) {
            strncpy(s_ap_password, ap_password, sizeof(s_ap_password) - 1);
        } else {
            s_ap_password[0] = '\0';
        }
    }

    if (sta_ssid) {
        strncpy(s_sta_ssid, sta_ssid, sizeof(s_sta_ssid) - 1);
        if (sta_password) {
            strncpy(s_sta_password, sta_password, sizeof(s_sta_password) - 1);
        } else {
            s_sta_password[0] = '\0';
        }
    }

    // WiFi HAL 초기화 (재초기화 허용)
    esp_err_t ret = wifi_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to init WiFi HAL: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    // 이벤트 핸들러 등록 (재등록 허용)
    wifi_hal_register_event_handler(eventHandler);

    // 기존 netif 정리 (재초기화 시)
    if (s_netif_sta && !s_sta_enabled) {
        // STA 비활성화: 기존 STA netif 해제
        T_LOGI(TAG, "STA disabled: destroying existing STA netif");
        esp_netif_destroy(s_netif_sta);
        s_netif_sta = nullptr;
    }
    if (s_netif_ap && !s_ap_enabled) {
        // AP 비활성화: 기존 AP netif 해제
        T_LOGI(TAG, "AP disabled: destroying existing AP netif");
        esp_netif_destroy(s_netif_ap);
        s_netif_ap = nullptr;
    }

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
    if (mode == WIFI_MODE_NULL) {
        T_LOGI(TAG, "WiFi mode: NULL (both AP/STA disabled)");
    }

    // AP netif 및 설정
    if (s_ap_enabled) {
        if (!s_netif_ap) {
            s_netif_ap = (esp_netif_t*)wifi_hal_create_ap_netif();
            if (!s_netif_ap) {
                T_LOGE(TAG, "Failed to create AP netif");
                return ESP_FAIL;
            }
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
        if (!s_netif_sta) {
            s_netif_sta = (esp_netif_t*)wifi_hal_create_sta_netif();
            if (!s_netif_sta) {
                T_LOGE(TAG, "Failed to create STA netif");
                // AP netif 정리
                if (s_netif_ap) {
                    esp_netif_destroy(s_netif_ap);
                    s_netif_ap = nullptr;
                }
                return ESP_FAIL;
            }
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
        T_LOGE(TAG, "Failed to start WiFi: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    s_initialized = true;

    T_LOGI(TAG, "WiFi Driver initialized");
    T_LOGI(TAG, "  AP: %s (%s)", s_ap_enabled ? s_ap_ssid : "disabled",
        s_ap_password[0] != '\0' ? "secured" : "open");
    T_LOGI(TAG, "  STA: %s (%s)", s_sta_enabled ? s_sta_ssid : "disabled",
        s_sta_password[0] != '\0' ? "secured" : "open");

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

    T_LOGI(TAG, "Deinitializing WiFi Driver");

    wifi_hal_stop();
    wifi_hal_deinit();  // netif는 해제하지 않음 (재사용)

    // netif 포인터 유지 (재사용을 위해 NULL로 설정하지 않음)

    s_initialized = false;
    s_ap_started = false;
    s_sta_connected = false;

    T_LOGI(TAG, "WiFi Driver deinitialized");
    return ESP_OK;
}

/**
 * @brief WiFi Driver 재설정 (deinit+init 없이 stop+start만으로 재설정)
 *
 * esp_wifi_deinit()은 driver 구조체를 파괴하여 netif 참조 무효화 문제를 일으킴.
 * 이 함수는 driver를 유지하면서 설정만 변경하여 재시작한다.
 *
 * @param ap_ssid AP SSID (비활성화 시 nullptr)
 * @param ap_password AP 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @param sta_ssid STA SSID (비활성화 시 nullptr)
 * @param sta_password STA 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t WiFiDriver::reconfigure(const char* ap_ssid, const char* ap_password,
    const char* sta_ssid, const char* sta_password)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Reconfigure failed: not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Reconfiguring WiFi Driver");

    // 새 설정 저장
    bool new_ap_enabled = (ap_ssid != nullptr);
    bool new_sta_enabled = (sta_ssid != nullptr);

    // 기존 netif 상태 확인
    bool has_sta_netif = (s_netif_sta != nullptr);
    bool has_ap_netif = (s_netif_ap != nullptr);

    T_LOGD(TAG, "  Existing netif: STA=%d, AP=%d", has_sta_netif, has_ap_netif);
    T_LOGD(TAG, "  New config: STA=%d, AP=%d", new_sta_enabled, new_ap_enabled);

    // WiFi 모드 결정
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (new_ap_enabled && new_sta_enabled) {
        mode = WIFI_MODE_APSTA;
    } else if (new_ap_enabled) {
        mode = WIFI_MODE_AP;
    } else if (new_sta_enabled) {
        mode = WIFI_MODE_STA;
    }

    // 모드가 NULL인 경우 (모두 비활성화)
    if (mode == WIFI_MODE_NULL) {
        T_LOGI(TAG, "WiFi mode: NULL (both AP/STA disabled)");

        // 먼저 STA 연결 해제 (연결된 경우)
        if (s_sta_connected) {
            wifi_hal_disconnect();
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // WiFi 정지
        wifi_hal_stop();

        // LwIP 스레드가 netif down 처리를 완료할 때까지 대기
        vTaskDelay(pdMS_TO_TICKS(200));

        // WiFi 모드 NULL 설정
        esp_wifi_set_mode(WIFI_MODE_NULL);

        // netif는 파괴하지 않음 (LwIP 충돌 방지)
        // 재활성화 시 기존 netif를 재사용
        T_LOGD(TAG, "netif preserved (STA=%p, AP=%p)", (void*)s_netif_sta, (void*)s_netif_ap);

        // 상태 업데이트
        s_ap_enabled = false;
        s_sta_enabled = false;
        s_ap_ssid[0] = '\0';
        s_sta_ssid[0] = '\0';

        T_LOGI(TAG, "WiFi Driver reconfigured (all disabled, netif preserved)");
        return ESP_OK;
    }

    // AP 설정
    if (new_ap_enabled) {
        if (!s_netif_ap) {
            s_netif_ap = (esp_netif_t*)wifi_hal_create_ap_netif();
            if (!s_netif_ap) {
                T_LOGE(TAG, "Failed to create AP netif");
                return ESP_FAIL;
            }
        }

        // AP SSID/Password 저장
        strncpy(s_ap_ssid, ap_ssid, sizeof(s_ap_ssid) - 1);
        s_ap_ssid[sizeof(s_ap_ssid) - 1] = '\0';
        if (ap_password) {
            strncpy(s_ap_password, ap_password, sizeof(s_ap_password) - 1);
            s_ap_password[sizeof(s_ap_password) - 1] = '\0';
        } else {
            s_ap_password[0] = '\0';
        }

        // AP 설정 적용
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
        s_ap_enabled = true;
    } else {
        // AP 비활성화: netif는 유지 (LwIP 충돌 방지)
        T_LOGD(TAG, "AP disabled (netif=%p preserved)", (void*)s_netif_ap);
        s_ap_enabled = false;
        s_ap_ssid[0] = '\0';
    }

    // STA 설정
    if (new_sta_enabled) {
        if (!s_netif_sta) {
            s_netif_sta = (esp_netif_t*)wifi_hal_create_sta_netif();
            if (!s_netif_sta) {
                T_LOGE(TAG, "Failed to create STA netif");
                return ESP_FAIL;
            }
        }

        // STA SSID/Password 저장
        strncpy(s_sta_ssid, sta_ssid, sizeof(s_sta_ssid) - 1);
        s_sta_ssid[sizeof(s_sta_ssid) - 1] = '\0';
        if (sta_password) {
            strncpy(s_sta_password, sta_password, sizeof(s_sta_password) - 1);
            s_sta_password[sizeof(s_sta_password) - 1] = '\0';
        } else {
            s_sta_password[0] = '\0';
        }

        // STA 설정 적용
        wifi_config_t sta_config = {};
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
        s_sta_enabled = true;
    } else {
        // STA 비활성화: netif는 유지 (LwIP 충돌 방지)
        T_LOGD(TAG, "STA disabled (netif=%p preserved)", (void*)s_netif_sta);
        s_sta_enabled = false;
        s_sta_ssid[0] = '\0';
    }

    // 현재 모드 확인
    wifi_mode_t current_mode;
    esp_wifi_get_mode(&current_mode);

    // 모드 변경이 필요한 경우만 stop/start
    if (current_mode != mode) {
        T_LOGD(TAG, "WiFi mode change: %d -> %d", current_mode, mode);

        // STA 연결 해제 (연결된 경우)
        if (s_sta_connected) {
            wifi_hal_disconnect();
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // WiFi 정지 (LwIP 스레드에서 netif down 처리됨)
        wifi_hal_stop();

        // LwIP 스레드가 netif down 처리를 완료할 때까지 대기
        vTaskDelay(pdMS_TO_TICKS(200));

        // 모드 변경
        esp_wifi_set_mode(mode);

        // WiFi 시작
        esp_err_t ret = wifi_hal_start();
        if (ret != ESP_OK) {
            T_LOGE(TAG, "Failed to start WiFi: %s (0x%x)", esp_err_to_name(ret), ret);
            return ret;
        }
    }

    T_LOGI(TAG, "WiFi Driver reconfigured");
    T_LOGI(TAG, "  AP: %s (%s)", s_ap_enabled ? s_ap_ssid : "disabled",
        s_ap_password[0] != '\0' ? "secured" : "open");
    T_LOGI(TAG, "  STA: %s (%s)", s_sta_enabled ? s_sta_ssid : "disabled",
        s_sta_password[0] != '\0' ? "secured" : "open");

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

    // STA RSSI 갱신 (현재 미사용 - 주석 처리)
    // if (s_sta_connected) {
    //     wifi_ap_record_t ap_info;
    //     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    //         s_sta_rssi = ap_info.rssi;
    //         status.sta_rssi = s_sta_rssi;
    //     }
    // }

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

    T_LOGI(TAG, "STA reconnecting...");
    s_sta_retry_count = 0;
    s_sta_auth_failed = false;  // 인증 실패 플래그 리셋
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

    T_LOGI(TAG, "STA disconnecting");
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
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid) {
        T_LOGE(TAG, "SSID is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGI(TAG, "Reconfiguring STA: SSID=%s", ssid);
    T_LOGD(TAG, "  Current state: s_ap_enabled=%d, s_sta_enabled=%d, s_netif_ap=%p, s_netif_sta=%p",
            s_ap_enabled, s_sta_enabled, (void*)s_netif_ap, (void*)s_netif_sta);

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

    // 현재 WiFi 모드 확인
    wifi_mode_t current_mode;
    esp_err_t mode_ret = esp_wifi_get_mode(&current_mode);

    // WiFi가 NULL 모드이거나 정지된 경우, 시작해야 함
    bool needs_start = (mode_ret != ESP_OK) || (current_mode == WIFI_MODE_NULL);
    bool needs_apsta = (s_ap_enabled || (s_netif_ap != nullptr));
    bool needs_mode_change = (current_mode == WIFI_MODE_AP) || (current_mode == WIFI_MODE_STA);

    T_LOGD(TAG, "  WiFi state: mode_ret=%d, current_mode=%d, needs_start=%d, needs_apsta=%d, needs_mode_change=%d",
            mode_ret, current_mode, needs_start, needs_apsta, needs_mode_change);

    if (needs_start) {
        T_LOGI(TAG, "WiFi stopped, restart needed (mode_ret=%d, current_mode=%d)",
                mode_ret, current_mode);

        // WiFi 모드 설정 (AP가 있으면 APSTA, 없으면 STA)
        wifi_mode_t new_mode = needs_apsta ? WIFI_MODE_APSTA : WIFI_MODE_STA;
        esp_wifi_set_mode(new_mode);
        vTaskDelay(pdMS_TO_TICKS(50));

        // WiFi 시작
        esp_err_t start_ret = wifi_hal_start();
        if (start_ret != ESP_OK) {
            T_LOGE(TAG, "Failed to start WiFi: %s (0x%x)", esp_err_to_name(start_ret), start_ret);
            return start_ret;
        }
        T_LOGI(TAG, "WiFi restarted (mode: %s)", needs_apsta ? "APSTA" : "STA");
        vTaskDelay(pdMS_TO_TICKS(100));
    } else if (needs_mode_change) {
        // AP → APSTA 또는 STA → APSTA 모드 변경 필요
        T_LOGI(TAG, "WiFi mode change needed: %d -> APSTA", current_mode);

        wifi_mode_t new_mode = WIFI_MODE_APSTA;
        esp_wifi_set_mode(new_mode);
        vTaskDelay(pdMS_TO_TICKS(100));

        T_LOGI(TAG, "WiFi mode changed: APSTA");
    } else if (!s_netif_sta) {
        // STA netif가 없는 경우 (AP 전용 모드에서 STA 활성화)
        T_LOGI(TAG, "Creating STA netif (AP -> APSTA mode transition)");

        // STA netif 생성 (모드 변경 전)
        s_netif_sta = (esp_netif_t*)wifi_hal_create_sta_netif();
        if (!s_netif_sta) {
            T_LOGE(TAG, "Failed to create STA netif");
            return ESP_FAIL;
        }

        // WiFi 모드를 APSTA로 변경
        esp_err_t mode_ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (mode_ret != ESP_OK) {
            T_LOGE(TAG, "Failed to change WiFi mode: %s (0x%x)", esp_err_to_name(mode_ret), mode_ret);
            // 생성된 netif 유지 (LwIP 충돌 방지)
            s_netif_sta = nullptr;
            return mode_ret;
        }

        T_LOGI(TAG, "STA netif created, switched to APSTA mode");
        vTaskDelay(pdMS_TO_TICKS(100));  // 모드 변경 안정화 대기
    }

    // STA 연결 해제 (이미 연결된 경우)
    if (s_sta_connected) {
        wifi_hal_disconnect();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

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
        T_LOGE(TAG, "Failed to set STA config: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    // STA 재연결 (재시도 카운트 및 인증 실패 플래그 초기화)
    s_sta_retry_count = 0;
    s_sta_auth_failed = false;  // 인증 실패 플래그 리셋

    ret = wifi_hal_connect();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to connect STA: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    T_LOGI(TAG, "STA reconfigured");
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
 * @brief WiFi Driver 재설정 (deinit+init 없이 stop+start만으로 재설정)
 *
 * esp_wifi_deinit()은 driver 구조체를 파괴하여 netif 참조 무효화 문제를 일으킴.
 * 이 함수는 driver를 유지하면서 설정만 변경하여 재시작한다.
 *
 * @param ap_ssid AP SSID (비활성화 시 nullptr)
 * @param ap_password AP 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @param sta_ssid STA SSID (비활성화 시 nullptr)
 * @param sta_password STA 비밀번호 (오픈 네트워크 시 빈 문자열)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_driver_reconfigure(const char* ap_ssid, const char* ap_password,
    const char* sta_ssid, const char* sta_password)
{
    return WiFiDriver::reconfigure(ap_ssid, ap_password, sta_ssid, sta_password);
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
