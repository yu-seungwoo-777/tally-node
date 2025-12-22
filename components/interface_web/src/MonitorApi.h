/**
 * @file MonitorApi.h
 * @brief 시스템 모니터링 REST API 핸들러
 *
 * API 역할:
 * - HTTP 요청/응답 처리
 * - JSON 직렬화
 * - SystemMonitor, NetworkManager 통합
 */

#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

/**
 * @brief 모니터링 API 핸들러 (정적 클래스)
 */
class MonitorApi {
public:
    /**
     * @brief 네트워크 상태 API
     *
     * GET /api/status
     * - WiFi AP/STA 상태
     * - Ethernet 상태
     */
    static esp_err_t statusHandler(httpd_req_t* req);

    /**
     * @brief 시스템 상태 API
     *
     * GET /api/system/health
     * - 업타임, CPU, 메모리
     * - 온도, 전압, 배터리
     */
    static esp_err_t healthHandler(httpd_req_t* req);

private:
    MonitorApi() = delete;
    ~MonitorApi() = delete;
    MonitorApi(const MonitorApi&) = delete;
    MonitorApi& operator=(const MonitorApi&) = delete;
};
