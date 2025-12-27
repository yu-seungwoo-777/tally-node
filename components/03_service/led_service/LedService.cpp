/**
 * @file LedService.cpp
 * @brief LED 서비스 구현 - ConfigService 색상을 WS2812Driver에 적용
 */

#include "LedService.h"
#include "ws2812_driver.h"
#include "ConfigService.h"
#include "t_log.h"
#include <cstring>

static const char* TAG = "LedService";

// 서비스 상태
static struct {
    bool initialized;
} s_service = {
    .initialized = false
};

// ============================================================================
// 색상 캐시 (ConfigService에서 로드)
// ============================================================================

static struct {
    uint8_t program_r, program_g, program_b;
    uint8_t preview_r, preview_g, preview_b;
    uint8_t off_r, off_g, off_b;
    uint8_t battery_low_r, battery_low_g, battery_low_b;
} s_colors = {
    // 기본값: R G OFF (빨강, 초록, 검정)
    .program_r = 255, .program_g = 0, .program_b = 0,
    .preview_r = 0, .preview_g = 255, .preview_b = 0,
    .off_r = 0, .off_g = 0, .off_b = 0,
    .battery_low_r = 255, .battery_low_g = 255, .battery_low_b = 0
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief ConfigService에서 색상 로드
 */
static void load_colors_from_nvs(void)
{
    config_service_get_led_program_color(&s_colors.program_r, &s_colors.program_g, &s_colors.program_b);
    config_service_get_led_preview_color(&s_colors.preview_r, &s_colors.preview_g, &s_colors.preview_b);
    config_service_get_led_off_color(&s_colors.off_r, &s_colors.off_g, &s_colors.off_b);
    config_service_get_led_battery_low_color(&s_colors.battery_low_r, &s_colors.battery_low_g, &s_colors.battery_low_b);

    T_LOGI(TAG, "색상 로드: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d) BAT(%d,%d,%d)",
             s_colors.program_r, s_colors.program_g, s_colors.program_b,
             s_colors.preview_r, s_colors.preview_g, s_colors.preview_b,
             s_colors.off_r, s_colors.off_g, s_colors.off_b,
             s_colors.battery_low_r, s_colors.battery_low_g, s_colors.battery_low_b);
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t led_service_init(int gpio_num, uint32_t num_leds, uint8_t camera_id)
{
    if (s_service.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "LED 서비스 초기화 중...");

    // ConfigService에서 색상 로드
    load_colors_from_nvs();

    // WS2812Driver 초기화
    esp_err_t ret = ws2812_driver_init(gpio_num, num_leds, camera_id);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "WS2812Driver 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    s_service.initialized = true;
    T_LOGI(TAG, "LED 서비스 초기화 완료");
    return ESP_OK;
}

void led_service_set_state(int state)
{
    if (!s_service.initialized) {
        return;
    }

    // 상태를 RGB로 변환 (ConfigService에서 로드한 색상 사용)
    uint8_t r, g, b;
    switch (state) {
    case 1:  // WS2812_PROGRAM
        r = s_colors.program_r;
        g = s_colors.program_g;
        b = s_colors.program_b;
        break;
    case 2:  // WS2812_PREVIEW
        r = s_colors.preview_r;
        g = s_colors.preview_g;
        b = s_colors.preview_b;
        break;
    case 4:  // WS2812_BATTERY_LOW
        r = s_colors.battery_low_r;
        g = s_colors.battery_low_g;
        b = s_colors.battery_low_b;
        break;
    default:  // WS2812_OFF
        r = s_colors.off_r;
        g = s_colors.off_g;
        b = s_colors.off_b;
        break;
    }

    ws2812_set_rgb(r, g, b);
}

void led_service_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_service.initialized) {
        return;
    }
    ws2812_set_rgb(r, g, b);
}

void led_service_set_brightness(uint8_t brightness)
{
    if (!s_service.initialized) {
        return;
    }
    ws2812_set_brightness(brightness);
}

void led_service_set_camera_id(uint8_t camera_id)
{
    if (!s_service.initialized) {
        return;
    }
    ws2812_set_camera_id(camera_id);
}

void led_service_off(void)
{
    if (!s_service.initialized) {
        return;
    }
    ws2812_off();
}

void led_service_deinit(void)
{
    if (!s_service.initialized) {
        return;
    }
    ws2812_deinit();
    s_service.initialized = false;
    T_LOGI(TAG, "LED 서비스 해제");
}

bool led_service_is_initialized(void)
{
    return s_service.initialized;
}

void led_service_load_colors(void)
{
    if (!s_service.initialized) {
        return;
    }
    load_colors_from_nvs();
    T_LOGI(TAG, "색상 다시 로드됨");
}

} // extern "C"
