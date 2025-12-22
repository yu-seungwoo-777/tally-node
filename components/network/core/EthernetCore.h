/**
 * @file EthernetCore.h
 * @brief W5500 SPI Ethernet Core API
 *
 * Core API 원칙:
 * - 하드웨어 추상화 (W5500 SPI Ethernet)
 * - 상태 최소화 (링크, IP 상태만 유지)
 * - 단일 책임 (이더넷 제어)
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "utils.h"

/* 이더넷 상태 */
struct EthernetStatus {
    bool initialized;
    bool link_up;
    bool got_ip;
    bool dhcp_mode;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char mac[18];
};

/**
 * @brief W5500 Ethernet Core API
 *
 * 설계 원칙:
 * - 상태: 링크 상태, IP 정보만 유지
 * - 스레드 안전성: ESP-IDF 이벤트 시스템 사용
 * - 성능: Cold Path (초기화, DHCP)
 */
class EthernetCore {
public:
    /**
     * @brief 초기화 및 W5500 시작
     *
     * @param dhcp_enabled DHCP 사용 여부
     * @param static_ip Static IP 주소 (DHCP 비활성화 시 사용)
     * @param static_netmask Netmask
     * @param static_gateway Gateway
     */
    static esp_err_t init(bool dhcp_enabled = true,
                         const char* static_ip = "192.168.0.100",
                         const char* static_netmask = "255.255.255.0",
                         const char* static_gateway = "192.168.0.1");

    /**
     * @brief 이더넷 상태 가져오기
     */
    static EthernetStatus getStatus();

    /**
     * @brief DHCP 활성화
     */
    static esp_err_t enableDHCP();

    /**
     * @brief Static IP 설정
     *
     * @param ip IP 주소
     * @param netmask Netmask
     * @param gateway Gateway
     */
    static esp_err_t enableStatic(const char* ip, const char* netmask, const char* gateway);

    /**
     * @brief 이더넷 재시작
     */
    static esp_err_t restart();

    /**
     * @brief 링크 상태 확인
     */
    static bool isLinkUp();

    /**
     * @brief IP 할당 여부 확인
     */
    static bool hasIP();

private:
    // 싱글톤 패턴
    EthernetCore() = delete;
    ~EthernetCore() = delete;
    EthernetCore(const EthernetCore&) = delete;
    EthernetCore& operator=(const EthernetCore&) = delete;

    // 내부 구현
    static void ethEventHandler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
    static void ipEventHandler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
    static void dhcpTimeoutTask(void* arg);

    // 상태 변수
    static esp_eth_handle_t s_eth_handle;
    static esp_netif_t* s_eth_netif;
    static bool s_initialized;
    static bool s_link_up;
    static bool s_got_ip;
    static bool s_dhcp_mode;
    static char s_ip[16];
    static char s_netmask[16];
    static char s_gateway[16];
    static char s_mac[18];
    static TaskHandle_t s_dhcp_timeout_task;

    // Static IP 설정 저장
    static char s_static_ip[16];
    static char s_static_netmask[16];
    static char s_static_gateway[16];
};
