/**
 * @file WiFiCore.h
 * @brief WiFi AP+STA 제어 Core API
 *
 * Core API 원칙:
 * - 하드웨어 추상화 (ESP32 WiFi)
 * - 상태 최소화 (이벤트 기반)
 * - 단일 책임 (WiFi AP/STA 제어)
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "utils.h"

/* WiFi 상태 */
struct WiFiStatus {
    bool ap_started;
    bool sta_connected;
    char ap_ip[16];
    char sta_ip[16];
    int8_t sta_rssi;
    uint8_t ap_clients;
};

/* WiFi 스캔 결과 */
struct WiFiScanResult {
    char ssid[33];
    uint8_t channel;
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
};

/**
 * @brief WiFi Core API
 *
 * 설계 원칙:
 * - 상태: 이벤트 그룹 + 연결 상태만 유지
 * - 스레드 안전성: FreeRTOS 이벤트 그룹 사용
 * - 성능: Cold Path (초기화, 스캔)
 */
class WiFiCore {
public:
    /**
     * @brief 초기화 및 AP+STA 모드 시작
     *
     * @param ap_ssid AP SSID
     * @param ap_password AP 비밀번호 (최소 8자)
     * @param sta_ssid STA SSID (nullptr이면 STA 비활성화)
     * @param sta_password STA 비밀번호
     */
    static esp_err_t init(const char* ap_ssid, const char* ap_password,
                         const char* sta_ssid = nullptr, const char* sta_password = nullptr);

    /**
     * @brief WiFi 상태 가져오기
     */
    static WiFiStatus getStatus();

    /**
     * @brief WiFi AP 스캔 (동기)
     *
     * @param out_results 스캔 결과 배열
     * @param max_results 배열 최대 크기
     * @param out_count 실제 발견된 AP 개수
     */
    static esp_err_t scan(WiFiScanResult* out_results, uint16_t max_results, uint16_t* out_count);

    /**
     * @brief STA 재연결 시도
     */
    static esp_err_t reconnectSTA();

    /**
     * @brief STA 연결 해제
     */
    static esp_err_t disconnectSTA();

    /**
     * @brief AP 클라이언트 수 가져오기
     */
    static uint8_t getAPClients();

    /**
     * @brief STA 연결 여부
     */
    static bool isSTAConnected();

private:
    // 싱글톤 패턴
    WiFiCore() = delete;
    ~WiFiCore() = delete;
    WiFiCore(const WiFiCore&) = delete;
    WiFiCore& operator=(const WiFiCore&) = delete;

    // 내부 구현
    static void eventHandler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data);
    static esp_err_t scanStart();
    static esp_err_t getScanResults(wifi_ap_record_t* out_ap_records,
                                   uint16_t max_records, uint16_t* out_count);

    // 상태 변수
    static EventGroupHandle_t s_event_group;
    static esp_netif_t* s_netif_ap;
    static esp_netif_t* s_netif_sta;
    static bool s_initialized;
    static bool s_ap_started;
    static bool s_sta_connected;
    static int s_sta_retry_num;
    static uint8_t s_ap_clients;

    // 이벤트 비트
    static constexpr int STA_CONNECTED_BIT = BIT0;
    static constexpr int STA_FAIL_BIT = BIT1;
    static constexpr int SCAN_DONE_BIT = BIT2;

    // 재시도 설정
    static constexpr int MAX_STA_RETRY = 5;
};
