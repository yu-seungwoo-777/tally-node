/**
 * @file WS2812Core.c
 * @brief WS2812 LED 제어 구현 (ESP-IDF RMT 드라이버 직접 사용)
 */

#include "WS2812Core.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "log.h"
#include "PinConfig.h"

#define TAG "WS2812"

// WS2812 타이밍 상수 (T = 100ns 기준) - 최적화된 타이밍
#define WS2812_T0H 2    // 0비트 하이 시간 (200ns) - 더 짧게
#define WS2812_T0L 9    // 0비트 로우 시간 (900ns)
#define WS2812_T1H 5    // 1비트 하이 시간 (500ns) - 더 짧게
#define WS2812_T1L 6    // 1비트 로우 시간 (600ns)
#define WS2812_RST 50   // 리셋 시간 (5000ns)

// RMT 설정
#define RMT_RESOLUTION_HZ 10000000  // 10MHz (100ns 단위)

static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_encoder_handle_t s_bytes_encoder = NULL;
static uint32_t s_num_leds = 8;
static uint8_t s_brightness = 255;
static ws2812_state_t s_current_state = WS2812_OFF;
static bool s_initialized = false;

esp_err_t WS2812Core_init(int gpio_num, uint32_t num_leds)
{
    if (s_initialized) {
        LOG_0(TAG, "WS2812 이미 초기화됨");
        return ESP_OK;
    }

    s_num_leds = num_leds;

    // RMT 채널 설정
    rmt_tx_channel_config_t tx_channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64, // 기본 64 심볼
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
        .flags.with_dma = false,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_channel_config, &s_tx_channel);
    if (ret != ESP_OK) {
        LOG_0(TAG, "RMT 채널 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // RMT 바이트 인코더 생성 (WS2812 타이밍 - TALLY_NODE와 유사하게 조정)
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = 3,  // T0H: 300ns (표준에 가깝게)
            .level0 = 1,
            .duration1 = 7,  // T0L: 700ns
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = 6,  // T1H: 600ns (표준에 가깝게)
            .level0 = 1,
            .duration1 = 5,  // T1L: 500ns
            .level1 = 0,
        },
        .flags.msb_first = 1  // MSB First
    };

    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &s_bytes_encoder);
    if (ret != ESP_OK) {
        LOG_0(TAG, "바이트 인코더 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // RMT 채널 활성화
    ret = rmt_enable(s_tx_channel);
    if (ret != ESP_OK) {
        LOG_0(TAG, "RMT 채널 활성화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    s_current_state = WS2812_OFF;
    s_initialized = true;

    LOG_0(TAG, "WS2812 초기화 완료 (GPIO %d, %lu개)", gpio_num, num_leds);
    return ESP_OK;
}

esp_err_t WS2812Core_initDefault(void)
{
    return WS2812Core_init(EORA_S3_LED_WS2812, 8);
}

void WS2812Core_setState(ws2812_state_t state)
{
    if (!s_initialized || !s_tx_channel || !s_bytes_encoder) {
        LOG_0(TAG, "WS2812 초기화 안됨");
        return;
    }

    if (s_current_state == state) {
        return;
    }

    s_current_state = state;

    // LED 데이터 생성 (GRB 순서 - WS2812B는 GRB 순서를 기대함)
    uint8_t led_data[s_num_leds * 3];
    for (uint32_t i = 0; i < s_num_leds; i++) {
        uint8_t r = 0, g = 0, b = 0;

        switch (state) {
        case WS2812_OFF:
            break;
        case WS2812_PROGRAM:
            r = s_brightness;  // Program = Red
            break;
        case WS2812_PREVIEW:
            g = s_brightness;  // Preview = Green
            break;
        }

        // WS2812B는 GRB 순서
        led_data[i * 3] = g;     // Green first
        led_data[i * 3 + 1] = r; // Red second
        led_data[i * 3 + 2] = b; // Blue third
    }

    // RMT로 LED 데이터 전송
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, led_data, sizeof(led_data), &tx_config);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WS2812 LED 데이터 전송 실패: %s", esp_err_to_name(ret));
        return;
    }

    // 전송 완료 대기 (최대 1ms 타임아웃 - DMA로 빠름)
    rmt_tx_wait_all_done(s_tx_channel, pdMS_TO_TICKS(1));

    // WS2812 리셋 신호를 위한 짧은 지연 (50us)
    esp_rom_delay_us(50);  // 50us 지연 (WS2812 최소 리셋 요구사항)

    LOG_1(TAG, "LED 상태 변경: %d", state);
}

void WS2812Core_setLedState(uint32_t led_index, ws2812_state_t state)
{
    if (!s_initialized || !s_tx_channel || !s_bytes_encoder) {
        LOG_0(TAG, "WS2812 초기화 안됨");
        return;
    }

    if (led_index >= s_num_leds) {
        LOG_0(TAG, "LED 인덱스 초과: %d >= %d", led_index, s_num_leds);
        return;
    }

    // LED 데이터 생성 (GRB 순서)
    uint8_t led_data[s_num_leds * 3];

    // 모든 LED를 현재 상태로 초기화
    for (uint32_t i = 0; i < s_num_leds; i++) {
        uint8_t r = 0, g = 0, b = 0;

        // 지정된 LED만 새로운 상태로 설정
        if (i == led_index) {
            switch (state) {
            case WS2812_OFF:
                break;
            case WS2812_PROGRAM:
                r = s_brightness;  // Program = Red
                break;
            case WS2812_PREVIEW:
                g = s_brightness;  // Preview = Green
                break;
            }
        } else {
            // 다른 LED들은 현재 상태 유지 (단순화를 위해 모두 OFF)
            // TODO: 실제로는 각 LED의 현재 상태를 저장하고 있어야 함
        }

        // WS2812B는 GRB 순서
        led_data[i * 3] = g;     // Green first
        led_data[i * 3 + 1] = r; // Red second
        led_data[i * 3 + 2] = b; // Blue third
    }

    // RMT로 LED 데이터 전송
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, led_data, sizeof(led_data), &tx_config);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WS2812 LED 데이터 전송 실패: %s", esp_err_to_name(ret));
        return;
    }

    // 전송 완료 대기
    rmt_tx_wait_all_done(s_tx_channel, pdMS_TO_TICKS(1));

    // WS2812 리셋 신호
    esp_rom_delay_us(50);

    LOG_1(TAG, "LED[%d] 상태 변경: %d", led_index, state);
}

void WS2812Core_setLedStates(const ws2812_state_t* states, uint32_t count)
{
    if (!s_initialized || !s_tx_channel || !s_bytes_encoder) {
        LOG_0(TAG, "WS2812 초기화 안됨");
        return;
    }

    if (!states || count == 0) {
        return;
    }

    // LED 데이터 생성 (GRB 순서)
    uint8_t led_data[s_num_leds * 3];

    for (uint32_t i = 0; i < s_num_leds; i++) {
        uint8_t r = 0, g = 0, b = 0;

        ws2812_state_t state = (i < count) ? states[i] : WS2812_OFF;

        switch (state) {
        case WS2812_OFF:
            break;
        case WS2812_PROGRAM:
            r = s_brightness;  // Program = Red
            break;
        case WS2812_PREVIEW:
            g = s_brightness;  // Preview = Green
            break;
        }

        // WS2812B는 GRB 순서
        led_data[i * 3] = g;     // Green first
        led_data[i * 3 + 1] = r; // Red second
        led_data[i * 3 + 2] = b; // Blue third
    }

    // RMT로 LED 데이터 전송
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, led_data, sizeof(led_data), &tx_config);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WS2812 LED 데이터 전송 실패: %s", esp_err_to_name(ret));
        return;
    }

    // 전송 완료 대기
    rmt_tx_wait_all_done(s_tx_channel, pdMS_TO_TICKS(1));

    // WS2812 리셋 신호
    esp_rom_delay_us(50);

    LOG_1(TAG, "다중 LED 상태 변경 (%d개)", count);
}

void WS2812Core_off(void)
{
    WS2812Core_setState(WS2812_OFF);
}

void WS2812Core_setLedOff(uint32_t led_index)
{
    WS2812Core_setLedState(led_index, WS2812_OFF);
}

void WS2812Core_setBrightness(uint8_t brightness)
{
    if (brightness == 0) {
        brightness = 1;
    }

    if (s_brightness != brightness) {
        s_brightness = brightness;

        // 현재 상태 다시 적용
        ws2812_state_t current = s_current_state;
        s_current_state = WS2812_OFF;
        WS2812Core_setState(current);

        LOG_1(TAG, "밝기 변경: %d", brightness);
    }
}

void WS2812Core_deinit(void)
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
    LOG_0(TAG, "WS2812 해제 완료");
}