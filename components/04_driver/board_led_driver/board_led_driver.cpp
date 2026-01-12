/**
 * @file board_led_driver.cpp
 * @brief 내장 LED 드라이버 구현 (GPIO 37)
 */

#include "board_led_driver.h"
#include "PinConfig.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "t_log.h"

static const char* TAG = "04_BoardLed";
static bool s_initialized = false;
static esp_timer_handle_t s_timer = nullptr;

// 타이머 콜백 - LED 끄기
static void timer_callback(void* arg) {
    (void)arg;
    gpio_set_level(EORA_S3_LED_BOARD, 0);
}

extern "C" {

esp_err_t board_led_driver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // 타이머 생성
    esp_timer_create_args_t timer_args = {
        .callback = timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "led_timer"
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_timer);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "timer:fail:0x%x", ret);
        // 타이머 실패해도 LED는 사용 가능
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EORA_S3_LED_BOARD),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:0x%x", ret);
        return ret;
    }

    gpio_set_level(EORA_S3_LED_BOARD, 0);

    s_initialized = true;
    T_LOGD(TAG, "ok");

    return ESP_OK;
}

void board_led_driver_set_state(board_led_state_t state)
{
    if (!s_initialized) {
        return;
    }
    gpio_set_level(EORA_S3_LED_BOARD, state);
}

void board_led_driver_on(void)
{
    board_led_driver_set_state(BOARD_LED_ON);
}

void board_led_driver_off(void)
{
    board_led_driver_set_state(BOARD_LED_OFF);
}

void board_led_driver_toggle(void)
{
    if (!s_initialized) {
        return;
    }
    bool current = gpio_get_level(EORA_S3_LED_BOARD);
    gpio_set_level(EORA_S3_LED_BOARD, !current);
}

void board_led_driver_pulse(uint32_t duration_ms)
{
    if (!s_initialized) {
        return;
    }

    // LED 켜기
    gpio_set_level(EORA_S3_LED_BOARD, 1);

    // 타이머가 있으면 지정된 시간 후 끄기
    if (s_timer && duration_ms > 0) {
        esp_timer_stop(s_timer);  // 기존 타이머 중지
        esp_timer_start_once(s_timer, duration_ms * 1000);  // us 단위
    }
}

void board_led_driver_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_timer) {
        esp_timer_stop(s_timer);
        esp_timer_delete(s_timer);
        s_timer = nullptr;
    }

    gpio_reset_pin(EORA_S3_LED_BOARD);
    s_initialized = false;

    T_LOGD(TAG, "deinit");
}

} // extern "C"
