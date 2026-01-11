/**
 * @file DisplayDriver.cpp
 * @brief 디스플레이 드라이버 구현 (C++)
 *
 * U8g2 기반 SSD1306 OLED 디스플레이 드라이버 구현
 */

#include "display_driver.h"
#include "display_hal.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "04_DispDrv";

// U8g2 인스턴스
static u8g2_t s_u8g2;
static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;

esp_err_t DisplayDriver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // 뮤텍스 생성
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        T_LOGE(TAG, "fail:mutex");
        return ESP_FAIL;
    }

    // I2C 핀 설정
    u8g2_esp32_hal_t hal_config = U8G2_ESP32_HAL_DEFAULT;

    int sda, scl;
    display_hal_get_i2c_pins(&sda, &scl);
    hal_config.bus.i2c.sda = (gpio_num_t)sda;
    hal_config.bus.i2c.scl = (gpio_num_t)scl;

    // U8g2 HAL 초기화
    u8g2_esp32_hal_init(hal_config);

    // U8g2 초기화
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &s_u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    // 디스플레이 초기화
    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);

    // 화면 지우기
    u8g2_ClearBuffer(&s_u8g2);
    u8g2_SendBuffer(&s_u8g2);

    s_initialized = true;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

void DisplayDriver_setPower(bool on)
{
    if (s_initialized) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            u8g2_SetPowerSave(&s_u8g2, on ? 0 : 1);
            display_hal_set_power(on);
            xSemaphoreGive(s_mutex);
        }
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
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            u8g2_SendBuffer(&s_u8g2);
            xSemaphoreGive(s_mutex);
        }
    }
}

void DisplayDriver_sendBufferSync(void)
{
    if (s_initialized) {
        u8g2_SendBuffer(&s_u8g2);
    }
}

esp_err_t DisplayDriver_takeMutex(uint32_t timeout_ms)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

void DisplayDriver_giveMutex(void)
{
    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
    }
}

u8g2_t* DisplayDriver_getU8g2(void)
{
    return &s_u8g2;
}
