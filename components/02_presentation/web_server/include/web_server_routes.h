/**
 * @file web_server_routes.h
 * @brief Web Server URI 라우팅 테이블 (X-Macro 패턴)
 */

#ifndef TALLY_WEB_SERVER_ROUTES_H
#define TALLY_WEB_SERVER_ROUTES_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// 라우트 배열 (web_server_routes.cpp에서 정의)
extern const httpd_uri_t g_routes[];
extern const size_t g_route_count;

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_ROUTES_H
