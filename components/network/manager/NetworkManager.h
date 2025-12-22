/**
 * @file NetworkManager.h
 * @brief 네트워크 통합 관리 Manager
 *
 * Manager 역할:
 * - Core API 통합 (WiFiCore + EthernetCore + ConfigCore)
 * - 비즈니스 로직 (인터페이스 우선순위, 상태 모니터링)
 * - 상태 관리 (Stateful)
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "ConfigCore.h"
#include "WiFiCore.h"
#include "EthernetCore.h"

/* 네트워크 인터페이스 타입 */
enum class NetworkInterface {
    WIFI_AP = 0,
    WIFI_STA,
    ETHERNET,
    MAX
};

/* 인터페이스 상태 */
struct NetworkIfStatus {
    bool active;
    bool connected;
    char ip[16];
    char netmask[16];
    char gateway[16];
};

/* 전체 네트워크 상태 */
struct NetworkStatus {
    NetworkIfStatus wifi_ap;
    NetworkIfStatus wifi_sta;
    NetworkIfStatus ethernet;
    WiFiStatus wifi_detail;
    EthernetStatus eth_detail;
};

/**
 * @brief 네트워크 통합 관리 Manager
 *
 * 설계 원칙:
 * - 상태: 여러 Core API 상태를 통합 관리
 * - 비즈니스 로직: 인터페이스 우선순위, 장애 조치
 * - Core API 조율: WiFiCore, EthernetCore, ConfigCore 사용
 */
class NetworkManager {
public:
    /**
     * @brief 초기화
     *
     * ConfigCore에서 설정을 읽어 WiFiCore, EthernetCore를 초기화합니다.
     */
    static esp_err_t init();

    /**
     * @brief 전체 네트워크 상태 가져오기
     */
    static NetworkStatus getStatus();

    /**
     * @brief 네트워크 상태 출력 (로그)
     */
    static void printStatus();

    /**
     * @brief 초기화 여부 확인
     */
    static bool isInitialized();

    /**
     * @brief WiFi 재시작
     *
     * 설정 변경 후 WiFi를 재시작합니다.
     */
    static esp_err_t restartWiFi();

    /**
     * @brief Ethernet 재시작
     *
     * 설정 변경 후 Ethernet을 재시작합니다.
     */
    static esp_err_t restartEthernet();

private:
    // 싱글톤 패턴
    NetworkManager() = delete;
    ~NetworkManager() = delete;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // 상태 변수
    static bool s_initialized;
};
