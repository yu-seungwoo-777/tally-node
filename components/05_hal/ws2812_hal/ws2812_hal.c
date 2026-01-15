/**
 * @file ws2812_hal.c
 * @brief WS2812 HAL 구현
 *
 * ESP32-S3 RMT(Remote Control) 드라이버를 사용하여 WS2812 RGB LED를 제어합니다.
 * - WS2812 타이밍: T0H(300ns), T0L(700ns), T1H(600ns), T1L(500ns)
 * - RMT 해상도: 10MHz (100ns 단위)
 * - DMA 비활성화 (polling 모드)
 */

#include "ws2812_hal.h"
#include "error_macros.h"
#include "driver/rmt_tx.h"
#include "esp_rom_sys.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "05_Ws2812";

// ============================================================================
// WS2812 타이밍 상수
// ============================================================================

/**
 * @brief WS2812 데이터 비트 타이밍 (단위: 100ns)
 *
 * WS2812 프로토콜 타이밍 요구사항:
 * - T0H: 0비트 하이 레벨 (350ns ±150ns)
 * - T0L: 0비트 로우 레벨 (값으로 계산)
 * - T1H: 1비트 하이 레벨 (700ns ±150ns)
 * - T1L: 1비트 로우 레벨 (값으로 계산)
 *
 * RMT 해상도 10MHz에서 1단위 = 100ns
 */
#define WS2812_T0H_DURATION    3   // T0H: 300ns
#define WS2812_T0L_DURATION    7   // T0L: 700ns
#define WS2812_T1H_DURATION    6   // T1H: 600ns
#define WS2812_T1L_DURATION    5   // T1L: 500ns

/**
 * @brief RMT 해상도 (Hz)
 *
 * 10MHz = 100ns 단위. WS2812 타이밍 정밀도를 위해 충분한 해상도.
 */
#define RMT_RESOLUTION_HZ      10000000  // 10MHz

/**
 * @brief WS2812 리셋 신호 시간 (us)
 *
 * WS2812는 데이터 전송 후 50us 이상의 로우 레벨을 유지해야 리셋됨.
 */
#define WS2812_RESET_DURATION_US   50

/**
 * @brief RMT 전송 완료 대기 시간 (ms)
 */
#define RMT_TX_WAIT_TIMEOUT_MS     10

// ============================================================================
// 내부 상태 변수
// ============================================================================

/** RMT TX 채널 핸들 */
static rmt_channel_handle_t s_tx_channel = NULL;

/** RMT 바이트 인코더 핸들 */
static rmt_encoder_handle_t s_bytes_encoder = NULL;

/** LED 개수 */
static uint32_t s_num_leds = 1;

/** 초기화 완료 여부 */
static bool s_initialized = false;

// ============================================================================
// 공개 API 구현
// ============================================================================

/**
 * @brief WS2812 HAL 초기화
 *
 * RMT 채널과 바이트 인코더를 초기화하여 WS2812 LED를 제어합니다.
 * 이미 초기화된 경우 무시합니다.
 *
 * @param gpio_num WS2812 데이터 핀 GPIO 번호
 * @param num_leds LED 개수
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ws2812_hal_init(int gpio_num, uint32_t num_leds)
{
    T_LOGD(TAG, "init:gpio=%d,n=%lu", gpio_num, num_leds);

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
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
        T_LOGE(TAG, "fail:chan:0x%x", ret);
        return ret;
    }

    // RMT 바이트 인코더
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .duration0 = WS2812_T0H_DURATION,
            .level0 = 1,
            .duration1 = WS2812_T0L_DURATION,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = WS2812_T1H_DURATION,
            .level0 = 1,
            .duration1 = WS2812_T1L_DURATION,
            .level1 = 0,
        },
        .flags.msb_first = 1
    };

    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &s_bytes_encoder);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:enc:0x%x", ret);
        rmt_del_channel(s_tx_channel);
        s_tx_channel = NULL;
        return ret;
    }

    // RMT 채널 활성화
    ret = rmt_enable(s_tx_channel);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:enable:0x%x", ret);
        rmt_del_encoder(s_bytes_encoder);
        rmt_del_channel(s_tx_channel);
        s_bytes_encoder = NULL;
        s_tx_channel = NULL;
        return ret;
    }

    s_initialized = true;
    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief WS2812 데이터 전송
 *
 * RGB 데이터를 WS2812 LED로 전송합니다.
 * 데이터 형식: GRB 순서 (LED당 3바이트)
 *
 * @param data 전송할 데이터 버퍼 (GRB 순서)
 * @param length 데이터 길이 (LED 개수 * 3)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ws2812_hal_transmit(const uint8_t* data, size_t length)
{
    RETURN_ERR_IF_NOT_INIT(s_initialized);
    RETURN_ERR_IF_NULL(data);

    if (length == 0) {
        T_LOGE(TAG, "fail:len=0");
        return ESP_ERR_INVALID_ARG;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, data, length, &tx_config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:tx:0x%x", ret);
        return ret;
    }

    // 전송 완료 대기
    ret = rmt_tx_wait_all_done(s_tx_channel, pdMS_TO_TICKS(RMT_TX_WAIT_TIMEOUT_MS));
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:wait:0x%x", ret);
        return ret;
    }

    // WS2812 리셋 신호
    esp_rom_delay_us(WS2812_RESET_DURATION_US);

    T_LOGD(TAG, "ok:%zu", length);
    return ESP_OK;
}

/**
 * @brief WS2812 HAL 해제
 *
 * RMT 채널과 인코더를 정리합니다.
 */
void ws2812_hal_deinit(void)
{
    T_LOGD(TAG, "deinit");

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
}

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool ws2812_hal_is_initialized(void)
{
    return s_initialized;
}
