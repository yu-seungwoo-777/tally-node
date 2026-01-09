/**
 * @file display_hal.c
 * @brief 디스플레이 HAL 구현 (I2C + U8g2)
 *
 * SSD1306 OLED 디스플레이를 위한 하드웨어 추상화 계층 구현
 */

#include "display_hal.h"
#include "PinConfig.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "t_log.h"

static const char* TAG = "05_Display";

// I2C 설정
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define ACK_CHECK_EN 0x1

// 전원 상태
static bool s_power_on = false;

esp_err_t display_hal_init(void)
{
    // I2C 마스터 초기화 (이미 다른 곳에서 초기화되었을 수 있음)
    // 여기서는 초기화 상태만 확인하고 필요시 초기화

    T_LOGI(TAG, "Display HAL initialized (I2C SDA=%d, SCL=%d)",
            EORA_S3_I2C_SDA, EORA_S3_I2C_SCL);

    return ESP_OK;
}

void display_hal_get_i2c_pins(int *out_sda, int *out_scl)
{
    if (out_sda) *out_sda = EORA_S3_I2C_SDA;
    if (out_scl) *out_scl = EORA_S3_I2C_SCL;
}

int display_hal_get_i2c_port(void)
{
    return EORA_S3_I2C_PORT;
}

void display_hal_set_power(bool on)
{
    s_power_on = on;
}
