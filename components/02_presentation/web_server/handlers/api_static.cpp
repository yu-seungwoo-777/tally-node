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

// gzip 압축본 PSRAM 캐시 (주요 파일만)
static static_file_cache_t s_index_gz_cache = {nullptr, 0, false};
static static_file_cache_t s_css_gz_cache = {nullptr, 0, false};
static static_file_cache_t s_js_gz_cache = {nullptr, 0, false};


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
    T_LOGI(TAG, "Initializing PSRAM static file cache (gzip)...");

    // gzip 압축본을 PSRAM에 캐싱 (원본은 flash에서 fallback)
    cache_to_psram(index_html_gz_data, index_html_gz_len, &s_index_gz_cache, "index.html.gz");
    cache_to_psram(styles_css_gz_data, styles_css_gz_len, &s_css_gz_cache, "styles.css.gz");
    cache_to_psram(app_bundle_js_gz_data, app_bundle_js_gz_len, &s_js_gz_cache, "app.bundle.js.gz");
    size_t total_cached = 0;
    if (s_index_gz_cache.cached) total_cached += s_index_gz_cache.len;
    if (s_css_gz_cache.cached) total_cached += s_css_gz_cache.len;
    if (s_js_gz_cache.cached) total_cached += s_js_gz_cache.len;
    T_LOGI(TAG, "PSRAM cache complete: %zu KB (gzip)", total_cached / 1024);
}

void web_server_static_cache_deinit(void)
{
    if (s_index_gz_cache.cached && s_index_gz_cache.data) {
        heap_caps_free((void*)s_index_gz_cache.data);
        s_index_gz_cache.cached = false;
    }
    if (s_css_gz_cache.cached && s_css_gz_cache.data) {
        heap_caps_free((void*)s_css_gz_cache.data);
        s_css_gz_cache.cached = false;
    }
    if (s_js_gz_cache.cached && s_js_gz_cache.data) {
        heap_caps_free((void*)s_js_gz_cache.data);
        s_js_gz_cache.cached = false;
    }
}

// ============================================================================
// HTTP 핸들러
// ============================================================================

static void set_cache_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
}

// Accept-Encoding: gzip 지원 여부 확인
static bool client_accepts_gzip(httpd_req_t* req)
{
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Accept-Encoding");
    if (hdr_len == 0) {
        return false;
    }
    char buf[128];
    if (httpd_req_get_hdr_value_str(req, "Accept-Encoding", buf, sizeof(buf)) == ESP_OK) {
        return strstr(buf, "gzip") != nullptr;
    }
    return false;
}

// gzip 응답 전송 헬퍼 (캐시된 gz 또는 원본 fallback)
static void send_static_response(httpd_req_t* req, const char* content_type,
                                  const static_file_cache_t* gz_cache,
                                  const uint8_t* raw_data, size_t raw_len)
{
    httpd_resp_set_type(req, content_type);
    set_cache_headers(req);

    if (client_accepts_gzip(req) && gz_cache->cached) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_send(req, (const char*)gz_cache->data, gz_cache->len);
    } else {
        httpd_resp_send(req, (const char*)raw_data, raw_len);
    }
}

esp_err_t index_handler(httpd_req_t* req)
{
    send_static_response(req, "text/html", &s_index_gz_cache, index_html_data, index_html_len);
    return ESP_OK;
}

esp_err_t css_handler(httpd_req_t* req)
{
    send_static_response(req, "text/css", &s_css_gz_cache, styles_css_data, styles_css_len);
    return ESP_OK;
}

esp_err_t js_handler(httpd_req_t* req)
{
    send_static_response(req, "text/javascript", &s_js_gz_cache, app_bundle_js_data, app_bundle_js_len);
    return ESP_OK;
}

esp_err_t favicon_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

} // extern "C"
