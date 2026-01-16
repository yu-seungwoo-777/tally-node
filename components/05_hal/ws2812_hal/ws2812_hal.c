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
#include <string.h>

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
 * @brief WS2812 LED 데이터 버퍼 크기
 */
#define WS2812_BUFFER_SIZE    24  // LED 8개 x 3바이트 (GRB)

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

/** 전송 중 플래그 (Non-blocking 큐 방식) */
static volatile bool s_transmitting = false;

/** 대기 중인 데이터 버퍼 (전송 중 새 요청을 저장) */
static uint8_t s_pending_buffer[WS2812_BUFFER_SIZE];
static size_t s_pending_length = 0;

/** 대기 중인 데이터 플래그 */
static volatile bool s_has_pending = false;

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
 * @brief WS2812 데이터 전송 (Non-blocking + 큐 버퍼)
 *
 * RGB 데이터를 WS2812 LED로 전송합니다.
 * - 전송 중 새 요청이 들어오면 pending 버퍼에 저장 (최신 상태 유지)
 * - 다음 호출 때 pending 데이터를 자동으로 전송
 * - Non-blocking으로 즉시 반환 (약 0.25ms 전송 시간은 백그라운드에서 진행)
 *
 * @param data 전송할 데이터 버퍼 (GRB 순서)
 * @param length 데이터 길이 (LED 개수 * 3)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ws2812_hal_transmit(const uint8_t* data, size_t length)
{
    RETURN_ERR_IF_NOT_INIT(s_initialized);
    RETURN_ERR_IF_NULL(data);

    if (length == 0 || length > WS2812_BUFFER_SIZE) {
        T_LOGE(TAG, "fail:len=%zu", length);
        return ESP_ERR_INVALID_ARG;
    }

    // 1. 이전 전송 완료 확인 및 pending 처리
    if (s_transmitting) {
        // 전송 중: 현재 전송이 완료되었는지 확인
        esp_err_t ret = rmt_tx_wait_all_done(s_tx_channel, 0);  // Non-blocking 체크
        if (ret == ESP_OK) {
            // 전송 완료: 리셋 신호 후 플래그 해제
            esp_rom_delay_us(WS2812_RESET_DURATION_US);
            s_transmitting = false;
        } else {
            // 여전히 전송 중: pending 버퍼에 저장 (덮어쓰기 - 최신 상태)
            memcpy(s_pending_buffer, data, length);
            s_pending_length = length;
            s_has_pending = true;
            T_LOGD(TAG, "pending:%zu", length);
            return ESP_OK;  // Non-blocking으로 즉시 반환
        }
    }

    // 2. 전송 완료 상태: pending 데이터 먼저 처리
    const uint8_t* tx_data;
    size_t tx_length;

    if (s_has_pending) {
        // pending 버퍼에 새 데이터 덮어쓰기 (최신 상태)
        memcpy(s_pending_buffer, data, length);
        s_pending_length = length;
        s_has_pending = false;
        tx_data = s_pending_buffer;
        tx_length = s_pending_length;
    } else {
        tx_data = data;
        tx_length = length;
    }

    // 4. RMT 전송 시작 (Non-blocking)
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t ret = rmt_transmit(s_tx_channel, s_bytes_encoder, tx_data, tx_length, &tx_config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:tx:0x%x", ret);
        return ret;
    }

    s_transmitting = true;
    T_LOGD(TAG, "ok:%zu", tx_length);
    return ESP_OK;  // Non-blocking으로 즉시 반환
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
