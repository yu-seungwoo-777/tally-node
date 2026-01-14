/**
 * @file api_static.cpp
 * @brief API Static 핸들러 구현
 */

#include "api_static.h"
#include "static_files.h"

static void set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static void set_cache_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
}

extern "C" {

esp_err_t index_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_data, index_html_len);
    return ESP_OK;
}

esp_err_t css_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/css");
    set_cache_headers(req);
    httpd_resp_send(req, (const char*)styles_css_data, styles_css_len);
    return ESP_OK;
}

esp_err_t js_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    set_cache_headers(req);
    httpd_resp_send(req, (const char*)app_bundle_js_data, app_bundle_js_len);
    return ESP_OK;
}

esp_err_t alpine_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    set_cache_headers(req);
    httpd_resp_send(req, (const char*)alpine_js_data, alpine_js_len);
    return ESP_OK;
}

esp_err_t favicon_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t options_handler(httpd_req_t* req)
{
    set_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

} // extern "C"
