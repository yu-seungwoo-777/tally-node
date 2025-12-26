/**
 * @file ws2812_hal.c
 * @brief WS2812 HAL 구현 - RMT 드라이버 사용
 */

#include "ws2812_hal.h"
#include "driver/rmt_tx.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "WS2812Hal";

// RMT 설정
#define RMT_RESOLUTION_HZ  10000000  // 10MHz (100ns 단위)

// RMT 핸들
static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_bytes_encoder = NULL;
static uint32_t s_num_leds = 1;
static bool s_initialized = false;

esp_err_t ws2812_hal_init(int gpio_num, uint32_t num_leds)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WS2812 HAL 이미 초기화됨");
        return ESP_OK;
    }

    s_num_leds = num_leds;

    // RMT TX 채널 설정
    rmt_tx_channel_config_t tx_channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
        .flags.with_dma = false,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_channel_config, &s_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT 채널 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // RMT 바이트 인코더 (WS2812 타이밍)
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = 3,  // T0H: 300ns
            .level0 = 1,
            .duration1 = 7,  // T0L: 700ns
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = 6,  // T1H: 600ns
            .level0 = 1,
            .duration1 = 5,  // T1L: 500ns
            .level1 = 0,
        },
        .flags.msb_first = 1  // MSB First
    };

    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &s_bytes_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "바이트 인코더 생성 실패: %s", esp_err_to_name(ret));
        rmt_del_channel(s_tx_channel);
        s_tx_channel = NULL;
        return ret;
    }

    // RMT 채널 활성화
    ret = rmt_enable(s_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT 채널 활성화 실패: %s", esp_err_to_name(ret));
        rmt_del_encoder(s_bytes_encoder);
        rmt_del_channel(s_tx_channel);
        s_bytes_encoder = NULL;
        s_tx_channel = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WS2812 HAL 초기화 완료 (GPIO %d, %lu LEDs)", gpio_num, num_leds);
    return ESP_OK;
}

esp_err_t ws2812_hal_transmit(const uint8_t* data, size_t length)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (length != s_num_leds * 3) {
        ESP_LOGW(TAG, "데이터 길이 불일치: %zu != %lu", length, s_num_leds * 3);
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, data, length, &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT 전송 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 전송 완료 대기
    rmt_tx_wait_all_done(s_tx_channel, pdMS_TO_TICKS(10));

    // WS2812 리셋 신호 (50us)
    esp_rom_delay_us(50);

    return ESP_OK;
}

void ws2812_hal_deinit(void)
{
    if (s_tx_channel) {
        rmt_disable(s_tx_channel);
        rmt_del_channel(s_tx_channel);
        s_tx_channel = NULL;
    }

    if (s_bytes_encoder) {
        rmt_del_encoder(s_bytes_encoder);
        s_bytes_encoder = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "WS2812 HAL 해제 완료");
}

bool ws2812_hal_is_initialized(void)
{
    return s_initialized;
}
