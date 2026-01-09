/**
 * @file EthernetDriver.cpp
 * @brief Ethernet Driver 구현 (C++)
 */

#include "ethernet_driver.h"
#include "ethernet_hal.h"
#include "t_log.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include <cstring>
#include "lwip/dns.h"  // LwIP DNS (dns_setserver)

static const char* TAG = "04_Ethernet";

// ============================================================================
// EthernetDriver 클래스 (싱글톤)
// ============================================================================

class EthernetDriver {
public:
    // 상태 구조체
    struct Status {
        bool initialized = false;
        bool detected = false;
        bool link_up = false;
        bool got_ip = false;
        bool dhcp_mode = true;
        char ip[16] = {0};
        char netmask[16] = {0};
        char gateway[16] = {0};
        char mac[18] = {0};
    };

    // 네트워크 상태 변경 콜백 타입
    using NetworkCallback = void (*)(bool connected, const char* ip);

    // 초기화
    static esp_err_t init(bool dhcp_enabled,
                          const char* static_ip,
                          const char* static_netmask,
                          const char* static_gateway);

    // 정리
    static esp_err_t deinit(void);

    // 상태 조회
    static Status getStatus(void);
    static bool isInitialized(void) { return s_initialized; }
    static bool isLinkUp(void);
    static bool hasIP(void);

    // IP 모드 변경
    static esp_err_t enableDHCP(void);
    static esp_err_t enableStatic(const char* ip, const char* netmask, const char* gateway);

    // 제어
    static esp_err_t restart(void);

    // 네트워크 상태 변경 콜백 설정
    static void setNetworkCallback(NetworkCallback callback) { s_network_callback = callback; }

private:
    EthernetDriver() = delete;
    ~EthernetDriver() = delete;

    static void eventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data);

    // 정적 멤버
    static bool s_initialized;
    static bool s_dhcp_mode;
    static char s_static_ip[16];
    static char s_static_netmask[16];
    static char s_static_gateway[16];

    static NetworkCallback s_network_callback;
};

// ============================================================================
// 정적 멤버 초기화
// ============================================================================

bool EthernetDriver::s_initialized = false;
bool EthernetDriver::s_dhcp_mode = true;
char EthernetDriver::s_static_ip[16] = {0};
char EthernetDriver::s_static_netmask[16] = {0};
char EthernetDriver::s_static_gateway[16] = {0};

EthernetDriver::NetworkCallback EthernetDriver::s_network_callback = nullptr;

// ============================================================================
// 이벤트 핸들러
// ============================================================================

void EthernetDriver::eventHandler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
{
    if (event_base == ETH_EVENT) {
        if (event_id == ETHERNET_EVENT_CONNECTED) {
            T_LOGD(TAG, "Ethernet 링크 업");
        } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
            T_LOGW(TAG, "Ethernet 링크 다운");
            // 네트워크 상태 변경 콜백 호출 (연결 해제)
            if (s_network_callback) {
                s_network_callback(false, nullptr);
            }
        } else if (event_id == ETHERNET_EVENT_START) {
            T_LOGI(TAG, "Ethernet 시작됨");
        } else if (event_id == ETHERNET_EVENT_STOP) {
            T_LOGI(TAG, "Ethernet 정지됨");
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            auto* event = (ip_event_got_ip_t*) event_data;
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            T_LOGI(TAG, "Ethernet IP 획득: %s", ip_str);

            // DNS 서버 명시적 설정 (Google DNS, Cloudflare) - LwIP 직접 사용
            ip_addr_t dns_primary, dns_backup;
            dns_primary.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
            dns_primary.type = IPADDR_TYPE_V4;
            dns_backup.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
            dns_backup.type = IPADDR_TYPE_V4;

            // LwIP DNS 서버 직접 설정
            dns_setserver(0, &dns_primary);   // DNS_INDEX 0 = Primary
            dns_setserver(1, &dns_backup);    // DNS_INDEX 1 = Backup

            T_LOGI(TAG, "Ethernet DNS 서버 설정 (LwIP): 8.8.8.8 (Primary), 1.1.1.1 (Backup)");

            // 네트워크 상태 변경 콜백 호출 (연결 성공)
            if (s_network_callback) {
                s_network_callback(true, ip_str);
            }
        }
    }
}

// ============================================================================
// 초기화/정리
// ============================================================================

esp_err_t EthernetDriver::init(bool dhcp_enabled,
                               const char* static_ip,
                               const char* static_netmask,
                               const char* static_gateway)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "Ethernet Driver 초기화 중...");

    // 설정 저장
    s_dhcp_mode = dhcp_enabled;
    if (static_ip) {
        strncpy(s_static_ip, static_ip, sizeof(s_static_ip) - 1);
    }
    if (static_netmask) {
        strncpy(s_static_netmask, static_netmask, sizeof(s_static_netmask) - 1);
    }
    if (static_gateway) {
        strncpy(s_static_gateway, static_gateway, sizeof(s_static_gateway) - 1);
    }

    // Ethernet HAL 초기화
    esp_err_t ret = ethernet_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Ethernet HAL 초기화 실패");
        return ret;
    }

    // 이벤트 핸들러 등록
    ethernet_hal_register_event_handler(eventHandler);

    // Ethernet 시작
    ret = ethernet_hal_start();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Ethernet 시작 실패 (하드웨어 미장착 가능성): %s", esp_err_to_name(ret));
    }

    // IP 모드 설정
    if (s_dhcp_mode) {
        ethernet_hal_enable_dhcp();
    } else {
        ethernet_hal_enable_static(s_static_ip, s_static_netmask, s_static_gateway);
    }

    s_initialized = true;

    T_LOGI(TAG, "Ethernet Driver 초기화 완료");
    T_LOGI(TAG, "  모드: %s", s_dhcp_mode ? "DHCP" : "Static");
    if (!s_dhcp_mode) {
        T_LOGI(TAG, "  Static IP: %s", s_static_ip);
        T_LOGI(TAG, "  Netmask: %s", s_static_netmask);
        T_LOGI(TAG, "  Gateway: %s", s_static_gateway);
    }

    return ESP_OK;
}

esp_err_t EthernetDriver::deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Ethernet Driver 정리 중...");

    ethernet_hal_stop();
    ethernet_hal_deinit();

    s_initialized = false;

    T_LOGI(TAG, "Ethernet Driver 정리 완료");
    return ESP_OK;
}

// ============================================================================
// 상태 조회
// ============================================================================

EthernetDriver::Status EthernetDriver::getStatus(void)
{
    Status status;

    if (!s_initialized) {
        return status;
    }

    ethernet_hal_status_t hal_status;
    if (ethernet_hal_get_status(&hal_status) == ESP_OK) {
        status.initialized = hal_status.initialized;
        status.detected = hal_status.detected;
        status.link_up = hal_status.link_up;
        status.got_ip = hal_status.got_ip;
        status.dhcp_mode = s_dhcp_mode;
        strncpy(status.ip, hal_status.ip, sizeof(status.ip));
        strncpy(status.netmask, hal_status.netmask, sizeof(status.netmask));
        strncpy(status.gateway, hal_status.gateway, sizeof(status.gateway));
        strncpy(status.mac, hal_status.mac, sizeof(status.mac));
    }

    return status;
}

bool EthernetDriver::isLinkUp(void)
{
    if (!s_initialized) {
        return false;
    }
    return ethernet_hal_is_link_up();
}

bool EthernetDriver::hasIP(void)
{
    if (!s_initialized) {
        return false;
    }
    return ethernet_hal_has_ip();
}

// ============================================================================
// IP 모드 변경
// ============================================================================

esp_err_t EthernetDriver::enableDHCP(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "DHCP 모드로 전환");

    s_dhcp_mode = true;
    return ethernet_hal_enable_dhcp();
}

esp_err_t EthernetDriver::enableStatic(const char* ip, const char* netmask, const char* gateway)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Static IP 모드로 전환");

    s_dhcp_mode = false;
    if (ip) {
        strncpy(s_static_ip, ip, sizeof(s_static_ip) - 1);
    }
    if (netmask) {
        strncpy(s_static_netmask, netmask, sizeof(s_static_netmask) - 1);
    }
    if (gateway) {
        strncpy(s_static_gateway, gateway, sizeof(s_static_gateway) - 1);
    }

    return ethernet_hal_enable_static(s_static_ip, s_static_netmask, s_static_gateway);
}

// ============================================================================
// 제어
// ============================================================================

esp_err_t EthernetDriver::restart(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Ethernet 재시작...");

    return ethernet_hal_restart();
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t ethernet_driver_init(bool dhcp_enabled,
                               const char* static_ip,
                               const char* static_netmask,
                               const char* static_gateway)
{
    return EthernetDriver::init(dhcp_enabled, static_ip, static_netmask, static_gateway);
}

esp_err_t ethernet_driver_deinit(void)
{
    return EthernetDriver::deinit();
}

ethernet_driver_status_t ethernet_driver_get_status(void)
{
    auto cpp_status = EthernetDriver::getStatus();
    ethernet_driver_status_t c_status;
    c_status.initialized = cpp_status.initialized;
    c_status.detected = cpp_status.detected;
    c_status.link_up = cpp_status.link_up;
    c_status.got_ip = cpp_status.got_ip;
    c_status.dhcp_mode = cpp_status.dhcp_mode;
    strncpy(c_status.ip, cpp_status.ip, sizeof(c_status.ip));
    strncpy(c_status.netmask, cpp_status.netmask, sizeof(c_status.netmask));
    strncpy(c_status.gateway, cpp_status.gateway, sizeof(c_status.gateway));
    strncpy(c_status.mac, cpp_status.mac, sizeof(c_status.mac));
    return c_status;
}

bool ethernet_driver_is_initialized(void)
{
    return EthernetDriver::isInitialized();
}

bool ethernet_driver_is_link_up(void)
{
    return EthernetDriver::isLinkUp();
}

bool ethernet_driver_has_ip(void)
{
    return EthernetDriver::hasIP();
}

esp_err_t ethernet_driver_enable_dhcp(void)
{
    return EthernetDriver::enableDHCP();
}

esp_err_t ethernet_driver_enable_static(const char* ip, const char* netmask, const char* gateway)
{
    return EthernetDriver::enableStatic(ip, netmask, gateway);
}

esp_err_t ethernet_driver_restart(void)
{
    return EthernetDriver::restart();
}

void ethernet_driver_set_status_callback(ethernet_driver_status_callback_t callback)
{
    EthernetDriver::setNetworkCallback(callback);
}

} // extern "C"
