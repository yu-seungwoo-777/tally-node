/**
 * @file api_led.cpp
 * @brief API LED 핸들러 구현
 */

#include "api_led.h"
#include "web_server_helpers.h"
#include "web_server_cache.h"
#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "cJSON.h"

static const char* TAG = "02_WS_LED";

extern "C" {

esp_err_t api_led_colors_get_handler(httpd_req_t* req)
{
    T_LOGD(TAG, "GET /api/led/colors");
    web_server_set_cors_headers(req);

    // 캐시가 없으면 요청 이벤트 발행 (config_service에서 응답)
    if (!web_server_cache_is_led_colors_initialized()) {
        event_bus_publish(EVT_LED_COLORS_REQUEST, NULL, 0);
        // 응답 대기 (간단 구현을 위해 짧은 지연)
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    const web_server_led_colors_t* colors = web_server_cache_get_led_colors();

    httpd_resp_set_type(req, "application/json");
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"program\":{\"r\":%d,\"g\":%d,\"b\":%d},"
             "\"preview\":{\"r\":%d,\"g\":%d,\"b\":%d},"
             "\"off\":{\"r\":%d,\"g\":%d,\"b\":%d}}",
             colors->program.r, colors->program.g, colors->program.b,
             colors->preview.r, colors->preview.g, colors->preview.b,
             colors->off.r, colors->off.g, colors->off.b);
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

esp_err_t api_led_colors_post_handler(httpd_req_t* req)
{
    web_server_set_cors_headers(req);

    // 요청 크기 사전 검증
    if (!web_server_validate_content_length(req, 512)) {
        return ESP_FAIL;
    }

    // 요청 바디 읽기 (스택 할당)
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return web_server_send_json_bad_request(req, "Failed to read body");
    }
    buf[ret] = '\0';

    // JSON 파싱
    cJSON* root = cJSON_Parse(buf);
    if (root == nullptr) {
        T_LOGE(TAG, "POST /api/led/colors JSON parse failed");
        return web_server_send_json_bad_request(req, "Invalid JSON");
    }

    // 스택 할당 구조체 (재진입 가능)
    led_colors_event_t colors = {};

    // 기본값 (기존 색상 유지)
    cJSON* program = cJSON_GetObjectItem(root, "program");
    cJSON* preview = cJSON_GetObjectItem(root, "preview");
    cJSON* off = cJSON_GetObjectItem(root, "off");

    if (program) {
        cJSON* r = cJSON_GetObjectItem(program, "r");
        cJSON* g = cJSON_GetObjectItem(program, "g");
        cJSON* b = cJSON_GetObjectItem(program, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.program_r = (uint8_t)r->valueint;
            colors.program_g = (uint8_t)g->valueint;
            colors.program_b = (uint8_t)b->valueint;
        }
    }

    if (preview) {
        cJSON* r = cJSON_GetObjectItem(preview, "r");
        cJSON* g = cJSON_GetObjectItem(preview, "g");
        cJSON* b = cJSON_GetObjectItem(preview, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.preview_r = (uint8_t)r->valueint;
            colors.preview_g = (uint8_t)g->valueint;
            colors.preview_b = (uint8_t)b->valueint;
        }
    }

    if (off) {
        cJSON* r = cJSON_GetObjectItem(off, "r");
        cJSON* g = cJSON_GetObjectItem(off, "g");
        cJSON* b = cJSON_GetObjectItem(off, "b");
        if (r && g && b && cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            colors.off_r = (uint8_t)r->valueint;
            colors.off_g = (uint8_t)g->valueint;
            colors.off_b = (uint8_t)b->valueint;
        }
    }

    cJSON_Delete(root);

    // 색상 변경 이벤트 발행 (config_service에서 구독)
    event_bus_publish(EVT_LED_COLORS_CHANGED, &colors, sizeof(colors));

    T_LOGI(TAG, "LED colors changed: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d)",
             colors.program_r, colors.program_g, colors.program_b,
             colors.preview_r, colors.preview_g, colors.preview_b,
             colors.off_r, colors.off_g, colors.off_b);

    return web_server_send_json_ok(req);
}

} // extern "C"
