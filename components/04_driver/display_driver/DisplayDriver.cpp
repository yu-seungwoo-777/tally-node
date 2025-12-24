/**
 * @file DisplayDriver.cpp
 * @brief 디스플레이 드라이버 구현 (C++)
 *
 * U8g2 기반 SSD1306 OLED 디스플레이 드라이버 구현
 */

#include "DisplayDriver.h"
#include "display_hal.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "t_log.h"

static const char* TAG = "DISP_DRV";

// U8g2 인스턴스 (전역 변수로 u8g2_fonts.c에서 접근 가능)
static u8g2_t s_u8g2;
static bool s_initialized = false;

esp_err_t DisplayDriver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // I2C 핀 설정
    u8g2_esp32_hal_t hal_config = U8G2_ESP32_HAL_DEFAULT;

    int sda, scl;
    display_hal_get_i2c_pins(&sda, &scl);
    hal_config.bus.i2c.sda = (gpio_num_t)sda;
    hal_config.bus.i2c.scl = (gpio_num_t)scl;

    // U8g2 HAL 초기화
    u8g2_esp32_hal_init(hal_config);

    // U8g2 초기화 (SSD1306 128x64 I2C)
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &s_u8g2,
        U8G2_R0,                           // 회전 없음
        u8g2_esp32_i2c_byte_cb,            // I2C 콜백
        u8g2_esp32_gpio_and_delay_cb       // GPIO/Delay 콜백
    );

    // 디스플레이 초기화
    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);  // 전원 켜기

    // 화면 지우기
    u8g2_ClearBuffer(&s_u8g2);
    u8g2_SendBuffer(&s_u8g2);

    s_initialized = true;

    T_LOGI(TAG, "Display driver initialized (SSD1306 128x64)");

    return ESP_OK;
}

void DisplayDriver_setPower(bool on)
{
    if (s_initialized) {
        u8g2_SetPowerSave(&s_u8g2, on ? 0 : 1);
        display_hal_set_power(on);
    }
}

void DisplayDriver_clearBuffer(void)
{
    if (s_initialized) {
        u8g2_ClearBuffer(&s_u8g2);
    }
}

void DisplayDriver_sendBuffer(void)
{
    if (s_initialized) {
        u8g2_SendBuffer(&s_u8g2);
    }
}

u8g2_t* DisplayDriver_getU8g2(void)
{
    return &s_u8g2;
}
