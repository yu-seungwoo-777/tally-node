/**
 * @file board_led_driver.cpp
 * @brief 내장 LED 드라이버 구현 (GPIO 37)
 */

#include "board_led_driver.h"
#include "PinConfig.h"
#include "driver/gpio.h"
#include "t_log.h"

static const char* TAG = "04_BoardLed";
static bool s_initialized = false;

extern "C" {

esp_err_t board_led_driver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EORA_S3_LED_BOARD),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:0x%x", ret);
        return ret;
    }

    gpio_set_level(EORA_S3_LED_BOARD, 0);

    s_initialized = true;
    T_LOGD(TAG, "ok");

    return ESP_OK;
}

void board_led_set_state(board_led_state_t state)
{
    if (!s_initialized) {
        return;
    }
    gpio_set_level(EORA_S3_LED_BOARD, state);
}

void board_led_on(void)
{
    board_led_set_state(BOARD_LED_ON);
}

void board_led_off(void)
{
    board_led_set_state(BOARD_LED_OFF);
}

void board_led_toggle(void)
{
    if (!s_initialized) {
        return;
    }
    bool current = gpio_get_level(EORA_S3_LED_BOARD);
    gpio_set_level(EORA_S3_LED_BOARD, !current);
}

void board_led_driver_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    gpio_reset_pin(EORA_S3_LED_BOARD);
    s_initialized = false;

    T_LOGD(TAG, "deinit");
}

} // extern "C"
