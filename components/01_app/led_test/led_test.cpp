/**
 * @file led_test.cpp
 * @brief WS2812 LED 테스트 앱 구현
 */

#include "led_test.h"
#include "ws2812_driver.h"
#include "t_log.h"

static const char* TAG = "LEDTest";

// WS2812 핀 설정 (EoRa-S3)
#define WS2812_GPIO  45
#define NUM_LEDS     8

static uint32_t s_tick_count = 0;
static uint8_t s_current_demo = 0;

esp_err_t led_test_app_init(void)
{
    T_LOGI(TAG, "WS2812 LED 테스트 앱 초기화 중...");

    esp_err_t ret = ws2812_driver_init(WS2812_GPIO, NUM_LEDS, 0);  // camera_id 0=기본값
    if (ret != ESP_OK) {
        T_LOGE(TAG, "WS2812 드라이버 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 초기: 모두 OFF
    ws2812_off();

    T_LOGI(TAG, "LED 테스트 앱 초기화 완료");
    return ESP_OK;
}

void led_test_app_stop(void)
{
    ws2812_off();
    T_LOGI(TAG, "LED 테스트 앱 정지");
}

void led_test_app_deinit(void)
{
    ws2812_deinit();
    T_LOGI(TAG, "LED 테스트 앱 해제");
}

void led_test_app_tick(void)
{
    s_tick_count++;

    // 매 틱마다 데모 변경
    switch (s_current_demo) {
    case 0:
        // OFF
        ws2812_off();
        T_LOGI(TAG, "[0] OFF");
        break;
    case 1:
        // PROGRAM (빨강)
        ws2812_set_state(WS2812_PROGRAM);
        T_LOGI(TAG, "[1] PROGRAM (Red)");
        break;
    case 2:
        // PREVIEW (초록)
        ws2812_set_state(WS2812_PREVIEW);
        T_LOGI(TAG, "[2] PREVIEW (Green)");
        break;
    case 3:
        // LIVE (파랑)
        ws2812_set_state(WS2812_LIVE);
        T_LOGI(TAG, "[3] LIVE (Blue)");
        break;
    case 4:
        // BATTERY_LOW (노랑)
        ws2812_set_state(WS2812_BATTERY_LOW);
        T_LOGI(TAG, "[4] BATTERY_LOW (Yellow)");
        break;
    case 5:
        // RGB 직접 설정 (보라)
        ws2812_set_rgb(255, 0, 255);
        T_LOGI(TAG, "[5] RGB Custom (Purple)");
        break;
    case 6:
        // 밝기 50%
        ws2812_set_brightness(128);
        ws2812_set_state(WS2812_PROGRAM);
        T_LOGI(TAG, "[6] Brightness 50%%");
        break;
    case 7:
        // 밝기 100%
        ws2812_set_brightness(255);
        ws2812_set_state(WS2812_LIVE);
        T_LOGI(TAG, "[7] Brightness 100%%");
        break;
    }

    // 다음 데모
    s_current_demo++;
    if (s_current_demo > 7) {
        s_current_demo = 0;
    }
}
