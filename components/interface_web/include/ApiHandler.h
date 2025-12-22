/**
 * @file ApiHandler.h
 * @brief API 핸들러 통합 관리
 *
 * 설정 관련 API 엔드포인트 제공
 */

#pragma once

#include <esp_http_server.h>
#include <esp_err.h>

/**
 * @brief API 핸들러 클래스
 */
class ApiHandler
{
public:
    /**
     * @brief 전체 설정 조회 핸들러
     * GET /api/config
     *
     * 응답: { "wifi_sta": {...}, "wifi_ap": {...}, "ethernet": {...} }
     */
    static esp_err_t configGetHandler(httpd_req_t* req);

    /**
     * @brief WiFi 스캔 핸들러
     * GET /api/wifi/scan
     *
     * 응답: { "networks": [ { "ssid": "...", "rssi": -50, "auth": "WPA2", "channel": 1 }, ... ] }
     */
    static esp_err_t wifiScanHandler(httpd_req_t* req);

    /**
     * @brief WiFi 설정 핸들러
     * POST /api/config/wifi
     *
     * 요청: { "mode": "sta|ap", "ssid": "...", "password": "..." }
     */
    static esp_err_t configWifiHandler(httpd_req_t* req);

    /**
     * @brief Ethernet 설정 핸들러
     * POST /api/config/eth
     *
     * 요청: { "dhcp_enabled": true, "static_ip": "...", ... }
     */
    static esp_err_t configEthHandler(httpd_req_t* req);

    /**
     * @brief 스위처 목록 조회 핸들러
     * GET /api/config/switchers
     *
     * 응답: { "switchers": [ { "index": 0, "type": "ATEM", ... }, ... ] }
     */
    static esp_err_t configSwitchersGetHandler(httpd_req_t* req);

    /**
     * @brief 스위처 설정 핸들러
     * POST /api/config/switcher
     *
     * 요청: { "index": 0, "type": "ATEM", ... }
     */
    static esp_err_t configSwitcherSetHandler(httpd_req_t* req);

    /**
     * @brief 스위처 매핑 설정 핸들러
     * POST /api/config/switcher/mapping
     *
     * 요청: { "index": 0, "camera_limit": 10, "camera_offset": 0 }
     */
    static esp_err_t configSwitcherMappingHandler(httpd_req_t* req);

    /**
     * @brief 시스템 재시작 핸들러
     * POST /api/restart
     */
    static esp_err_t restartHandler(httpd_req_t* req);

    /**
     * @brief 스위처 연결 재시작 핸들러
     * POST /api/switcher/restart
     */
    static esp_err_t switcherRestartHandler(httpd_req_t* req);

    /**
     * @brief 듀얼 모드 설정 핸들러
     * POST /api/config/mode
     *
     * 요청: { "dual_mode": true/false }
     */
    static esp_err_t configModeHandler(httpd_req_t* req);

    /**
     * @brief LoRa 주파수 스캔 핸들러
     * GET /api/lora/scan?start=863&end=870&step=0.5
     *
     * 응답: { "channels": [ { "frequency": 863.0, "rssi": -95.5, "available": true }, ... ], "count": 14 }
     */
    static esp_err_t loraScanHandler(httpd_req_t* req);

    /**
     * @brief LoRa 설정 핸들러
     * POST /api/lora/config
     *
     * 요청: { "frequency": 868.0, "sync_word": 0x12 }
     */
    static esp_err_t loraConfigHandler(httpd_req_t* req);

    /**
     * @brief LoRa 상태 조회 핸들러
     * GET /api/lora/status
     *
     * 응답: { "frequency": 868.0, "rssi": -85.5, "snr": 9.5, "chip": "SX1262" }
     */
    static esp_err_t loraStatusHandler(httpd_req_t* req);

    /**
     * @brief LoRa 설정 적용 핸들러
     * POST /api/lora/apply
     *
     * 임시 저장된 설정을 실제로 적용 (3회 송신 + 1초 대기 + TX 변경)
     */
    static esp_err_t loraApplyHandler(httpd_req_t* req);

private:
    ApiHandler() = delete;  // 정적 클래스, 인스턴스 생성 불가
};
