/**
 * @file api_static.cpp
 * @brief API Static 핸들러 구현 (PSRAM 캐싱)
 */

#include "api_static.h"
#include "static_files.h"
#include "esp_heap_caps.h"
#include "t_log.h"

static const char* TAG = "02_WS_Static";

// ============================================================================
// PSRAM 캐싱 구조체
// ============================================================================

typedef struct {
    const uint8_t* data;  // PSRAM 포인터
    size_t len;
    bool cached;
} static_file_cache_t;

static static_file_cache_t s_index_cache = {nullptr, 0, false};
static static_file_cache_t s_css_cache = {nullptr, 0, false};
static static_file_cache_t s_js_cache = {nullptr, 0, false};
static static_file_cache_t s_alpine_cache = {nullptr, 0, false};

// ============================================================================
// PSRAM 캐싱 초기화
// ============================================================================

static bool cache_to_psram(const uint8_t* src_data, size_t src_len, static_file_cache_t* cache, const char* name)
{
    if (cache->cached) {
        return true;  // 이미 캐시됨
    }

    // PSRAM에 할당
    uint8_t* psram_data = (uint8_t*)heap_caps_malloc(src_len, MALLOC_CAP_SPIRAM);
    if (!psram_data) {
        T_LOGE(TAG, "Failed to allocate PSRAM for %s (%zu bytes)", name, src_len);
        return false;
    }

    // 데이터 복사
    memcpy(psram_data, src_data, src_len);
    cache->data = psram_data;
    cache->len = src_len;
    cache->cached = true;

    T_LOGI(TAG, "Cached %s to PSRAM: %zu bytes", name, src_len);
    return true;
}

extern "C" {

void web_server_static_cache_init(void)
{
    T_LOGI(TAG, "Initializing PSRAM static file cache...");

    // 정적 파일 PSRAM에 캐싱
    cache_to_psram(index_html_data, index_html_len, &s_index_cache, "index.html");
    cache_to_psram(styles_css_data, styles_css_len, &s_css_cache, "styles.css");
    cache_to_psram(app_bundle_js_data, app_bundle_js_len, &s_js_cache, "app.bundle.js");
    cache_to_psram(alpine_js_data, alpine_js_len, &s_alpine_cache, "alpine.js");

    size_t total_cached = 0;
    if (s_index_cache.cached) total_cached += s_index_cache.len;
    if (s_css_cache.cached) total_cached += s_css_cache.len;
    if (s_js_cache.cached) total_cached += s_js_cache.len;
    if (s_alpine_cache.cached) total_cached += s_alpine_cache.len;

    T_LOGI(TAG, "PSRAM cache complete: %zu KB", total_cached / 1024);
}

void web_server_static_cache_deinit(void)
{
    if (s_index_cache.cached && s_index_cache.data) {
        heap_caps_free((void*)s_index_cache.data);
        s_index_cache.cached = false;
    }
    if (s_css_cache.cached && s_css_cache.data) {
        heap_caps_free((void*)s_css_cache.data);
        s_css_cache.cached = false;
    }
    if (s_js_cache.cached && s_js_cache.data) {
        heap_caps_free((void*)s_js_cache.data);
        s_js_cache.cached = false;
    }
    if (s_alpine_cache.cached && s_alpine_cache.data) {
        heap_caps_free((void*)s_alpine_cache.data);
        s_alpine_cache.cached = false;
    }
}

// ============================================================================
// HTTP 핸들러
// ============================================================================

static void set_cache_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
}

esp_err_t index_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    set_cache_headers(req);
    const uint8_t* data = s_index_cache.cached ? s_index_cache.data : index_html_data;
    size_t len = s_index_cache.cached ? s_index_cache.len : index_html_len;
    httpd_resp_send(req, (const char*)data, len);
    return ESP_OK;
}

esp_err_t css_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/css");
    set_cache_headers(req);
    const uint8_t* data = s_css_cache.cached ? s_css_cache.data : styles_css_data;
    size_t len = s_css_cache.cached ? s_css_cache.len : styles_css_len;
    httpd_resp_send(req, (const char*)data, len);
    return ESP_OK;
}

esp_err_t js_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    set_cache_headers(req);
    const uint8_t* data = s_js_cache.cached ? s_js_cache.data : app_bundle_js_data;
    size_t len = s_js_cache.cached ? s_js_cache.len : app_bundle_js_len;
    httpd_resp_send(req, (const char*)data, len);
    return ESP_OK;
}

esp_err_t alpine_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/javascript");
    set_cache_headers(req);
    const uint8_t* data = s_alpine_cache.cached ? s_alpine_cache.data : alpine_js_data;
    size_t len = s_alpine_cache.cached ? s_alpine_cache.len : alpine_js_len;
    httpd_resp_send(req, (const char*)data, len);
    return ESP_OK;
}

esp_err_t favicon_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

} // extern "C"
