/**
 * @file WebServerCore.h
 * @brief HTTP 웹서버 및 REST API 코어 (C++)
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 웹서버 코어 (정적 클래스)
 *
 * HTTP 서버를 관리하고 REST API 엔드포인트를 제공합니다.
 * - 정적 리소스 서빙 (HTML, CSS, JS)
 * - 네트워크 상태 API
 * - 설정 관리 API
 * - 시스템 제어 API
 */
class WebServerCore {
public:
    /**
     * @brief 웹서버 초기화 및 시작
     * @return ESP_OK 성공, ESP_FAIL 실패
     */
    static esp_err_t init();

    /**
     * @brief 웹서버 중지
     * @return ESP_OK 성공, ESP_FAIL 실패
     */
    static esp_err_t stop();

    /**
     * @brief 웹서버 재시작
     * @return ESP_OK 성공, ESP_FAIL 실패
     */
    static esp_err_t restart();

    /**
     * @brief 웹서버 실행 여부 확인
     * @return true 실행 중, false 중지됨
     */
    static bool isRunning();

private:
    // 웹 리소스 핸들러들
    static esp_err_t indexHandler(httpd_req_t* req);
    static esp_err_t styleHandler(httpd_req_t* req);
    static esp_err_t appJsHandler(httpd_req_t* req);

    // 서버 핸들
    static httpd_handle_t s_server;
};
