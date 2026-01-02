/**
 * @file LedService.cpp
 * @brief LED 서비스 구현 - WS2812Driver + 내장 LED 제어
 */

#include "led_service.h"
#include "ws2812_driver.h"
#include "board_led_driver.h"
#include "t_log.h"
#include "event_bus.h"
#include <cstring>

static const char* TAG = "LedService";

// 서비스 상태
static struct {
    bool initialized;
    bool event_subscribed;  // 이벤트 구독 여부
} s_service = {
    .initialized = false,
    .event_subscribed = false
};

// ============================================================================
// 색상 캐시 (이벤트 기반 업데이트)
// ============================================================================

static led_colors_t s_colors = {
    // 기본값: R G OFF (빨강, 초록, 검정)
    .program_r = 255, .program_g = 0, .program_b = 0,
    .preview_r = 0, .preview_g = 255, .preview_b = 0,
    .off_r = 0, .off_g = 0, .off_b = 0,
    .battery_low_r = 255, .battery_low_g = 255, .battery_low_b = 0
};

// ============================================================================
// 이벤트 핸들러 (LED 색상 설정)
// ============================================================================

/**
 * @brief 설정 데이터 변경 이벤트 핸들러
 * LED 색상 설정 업데이트
 */
static esp_err_t on_config_data_event(const event_data_t* event)
{
    if (!event || event->type != EVT_CONFIG_DATA_CHANGED) {
        return ESP_OK;
    }

    const auto* config = reinterpret_cast<const config_data_event_t*>(event->data);
    if (config) {
        // config_data_event_t에는 LED 색상이 없으므로 기존 색상 유지
        // 향후 LED 색상이 이벤트에 추가되면 여기서 업데이트
        T_LOGD(TAG, "설정 데이터 이벤트 수신 (LED 색상은 별도 API로 설정)");
    }

    return ESP_OK;
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t led_service_init(int gpio_num, uint32_t num_leds, uint8_t camera_id)
{
    return led_service_init_with_colors(gpio_num, num_leds, camera_id, nullptr);
}

esp_err_t led_service_init_with_colors(int gpio_num, uint32_t num_leds, uint8_t camera_id, const led_colors_t* colors)
{
    if (s_service.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "LED 서비스 초기화 중...");

    // 색상 설정
    if (colors != nullptr) {
        memcpy(&s_colors, colors, sizeof(led_colors_t));
    }

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

esp_err_t led_service_set_colors(const led_colors_t* colors)
{
    if (colors == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_colors, colors, sizeof(led_colors_t));

    T_LOGI(TAG, "색상 설정: PGM(%d,%d,%d) PVW(%d,%d,%d) OFF(%d,%d,%d) BAT(%d,%d,%d)",
             s_colors.program_r, s_colors.program_g, s_colors.program_b,
             s_colors.preview_r, s_colors.preview_g, s_colors.preview_b,
             s_colors.off_r, s_colors.off_g, s_colors.off_b,
             s_colors.battery_low_r, s_colors.battery_low_g, s_colors.battery_low_b);

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

// ============================================================================
// 내장 LED 제어 (board_led_driver 위임)
// ============================================================================

esp_err_t led_service_init_board_led(void)
{
    return board_led_driver_init();
}

void led_service_deinit_board_led(void)
{
    board_led_driver_deinit();
}

void led_service_set_board_led_state(board_led_state_t state)
{
    board_led_set_state(state);
}

void led_service_board_led_on(void)
{
    board_led_on();
}

void led_service_board_led_off(void)
{
    board_led_off();
}

void led_service_toggle_board_led(void)
{
    board_led_toggle();
}

} // extern "C"
