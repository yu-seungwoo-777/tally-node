/**
 * @file PinConfig.h
 * @brief EoRa-S3 핀 맵 정의
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

// ============================================================================
// EoRa-S3 핀 맵
// ============================================================================

// I2C (OLED - 미사용)
#define EORA_S3_I2C_SDA         GPIO_NUM_18
#define EORA_S3_I2C_SCL         GPIO_NUM_17
#define EORA_S3_I2C_PORT        I2C_NUM_0
#define EORA_S3_I2C_FREQ_HZ     1000000  // 1MHz Fast Mode Plus

// SPI (OLED) - SPI3_HOST (별도 핀 사용)
#define EORA_S3_OLED_CLK         GPIO_NUM_14  // OLED 전용
#define EORA_S3_OLED_MOSI        GPIO_NUM_13  // OLED 전용
#define EORA_S3_OLED_CS          GPIO_NUM_21  // OLED 전용
#define EORA_S3_OLED_DC          GPIO_NUM_4   // OLED 전용
#define EORA_S3_OLED_RST         GPIO_NUM_10  // OLED 전용

// LoRa (SX1268) - SPI2_HOST
#define EORA_S3_LORA_MISO       GPIO_NUM_3
#define EORA_S3_LORA_MOSI       GPIO_NUM_6
#define EORA_S3_LORA_SCK        GPIO_NUM_5
#define EORA_S3_LORA_CS         GPIO_NUM_7
#define EORA_S3_LORA_DIO1       GPIO_NUM_33
#define EORA_S3_LORA_BUSY       GPIO_NUM_34
#define EORA_S3_LORA_RST        GPIO_NUM_8
#define EORA_S3_LORA_SPI_HOST   SPI2_HOST

// W5500 Ethernet - SPI3_HOST
#define EORA_S3_W5500_MISO      GPIO_NUM_15
#define EORA_S3_W5500_MOSI      GPIO_NUM_16
#define EORA_S3_W5500_SCK       GPIO_NUM_48
#define EORA_S3_W5500_CS        GPIO_NUM_47
#define EORA_S3_W5500_INT       -1            // 미사용
#define EORA_S3_W5500_RST       GPIO_NUM_12
#define EORA_S3_W5500_SPI_HOST  SPI3_HOST

// LED (Tally Light)
#define EORA_S3_LED_BOARD       GPIO_NUM_37   // 보드 내장 LED
#define EORA_S3_LED_PGM         GPIO_NUM_38   // Program LED (빨간색)
#define EORA_S3_LED_PVW         GPIO_NUM_39   // Preview LED (녹색)

// 버튼
#define EORA_S3_BUTTON          GPIO_NUM_0    // 보드 내장 버튼

// ADC
#define EORA_S3_BAT_ADC         GPIO_NUM_1    // 배터리 전압 측정

// WS2812 LED (RX 모드 전용)
#define EORA_S3_LED_WS2812      GPIO_NUM_45   // WS2812B 데이터 핀
