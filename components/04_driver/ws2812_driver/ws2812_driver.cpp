/**
 * @file ws2812_driver.cpp
 * @brief WS2812 Driver 구현 - RGB 변환, 상태 관리
 */

#include "ws2812_driver.h"
#include "ws2812_hal.h"
#include "event_bus.h"
#include "TallyTypes.h"
#include "t_log.h"
#include "PinConfig.h"

class WS2812Driver {
public:
    static esp_err_t init(int gpio_num, uint32_t num_leds, uint8_t camera_id);
    static void setState(ws2812_state_t state);
    static void setRgb(uint8_t r, uint8_t g, uint8_t b);
    static void setLedState(uint32_t led_index, ws2812_state_t state);
    static void setLedRgb(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b);
    static void setBrightness(uint8_t brightness);
    static void setCameraId(uint8_t camera_id);
    static void off(void);
    static void deinit(void);
    static bool isInitialized(void) { return s_initialized; }

private:
    WS2812Driver() = delete;
    ~WS2812Driver() = delete;

    // 상태를 RGB로 변환
    static void stateToRgb(ws2812_state_t state, uint8_t* r, uint8_t* g, uint8_t* b);

    // 이벤트 콜백
    static esp_err_t brightnessEventCallback(const event_data_t* event);
    static esp_err_t tallyEventCallback(const event_data_t* event);

    static bool s_initialized;
    static uint32_t s_num_leds;
    static uint8_t s_brightness;
    static uint8_t s_camera_id;
    static ws2812_state_t s_led_states[8];
};

bool WS2812Driver::s_initialized = false;
uint32_t WS2812Driver::s_num_leds = 1;
uint8_t WS2812Driver::s_brightness = 255;
uint8_t WS2812Driver::s_camera_id = 1;
ws2812_state_t WS2812Driver::s_led_states[8] = {WS2812_OFF};

esp_err_t WS2812Driver::init(int gpio_num, uint32_t num_leds, uint8_t camera_id)
{
    if (s_initialized) {
        T_LOGW("WS2812Drv", "이미 초기화됨");
        return ESP_OK;
    }

    // GPIO 미지정 시 PinConfig.h 사용
    if (gpio_num < 0) {
        gpio_num = EORA_S3_LED_WS2812;
    }

    // LED 개수는 드라이버 내부에서 8개 고정
    (void)num_leds;  // 미사용 경고 방지

    // 카메라 ID 설정 (0=기본값 1)
    if (camera_id == 0) {
        camera_id = 1;
    }
    s_camera_id = camera_id;

    esp_err_t ret = ws2812_hal_init(gpio_num, 8);
    if (ret != ESP_OK) {
        return ret;
    }

    s_num_leds = 8;
    s_brightness = 255;  // 기본값

    // 모든 LED OFF로 초기화 (상태)
    for (uint32_t i = 0; i < 8; i++) {
        s_led_states[i] = WS2812_OFF;
    }

    // 밝기 변경 이벤트 구독
    event_bus_subscribe(EVT_BRIGHTNESS_CHANGED, brightnessEventCallback);
    T_LOGI("WS2812Drv", "밝기 변경 이벤트 구독 완료");

    // Tally 상태 변경 이벤트 구독
    event_bus_subscribe(EVT_TALLY_STATE_CHANGED, tallyEventCallback);
    T_LOGI("WS2812Drv", "Tally 상태 변경 이벤트 구독 완료");

    s_initialized = true;
    off();  // 실제 LED OFF 상태로 전송 (초기화 완료 후)

    T_LOGI("WS2812Drv", "WS2812 드라이버 초기화 완료 (GPIO %d, %lu LEDs, 카메라 ID: %d)",
           gpio_num, s_num_leds, s_camera_id);
    return ESP_OK;
}

esp_err_t WS2812Driver::brightnessEventCallback(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    // 이벤트 데이터: uint8_t brightness (0-255)
    uint8_t new_brightness = *(const uint8_t*)event->data;

    if (s_brightness != new_brightness) {
        s_brightness = new_brightness;
        // 현재 상태 다시 적용
        for (uint32_t i = 0; i < s_num_leds; i++) {
            setLedState(i, s_led_states[i]);
        }
        T_LOGI("WS2812Drv", "이벤트 수신: 밝기 %d", new_brightness);
    }

    return ESP_OK;
}

void WS2812Driver::stateToRgb(ws2812_state_t state, uint8_t* r, uint8_t* g, uint8_t* b)
{
    switch (state) {
    case WS2812_OFF:
        *r = 0; *g = 0; *b = 0;
        break;
    case WS2812_PROGRAM:
        *r = 255; *g = 0; *b = 0;
        break;
    case WS2812_PREVIEW:
        *r = 0; *g = 255; *b = 0;
        break;
    case WS2812_LIVE:
        *r = 0; *g = 0; *b = 255;
        break;
    case WS2812_BATTERY_LOW:
        *r = 255; *g = 255; *b = 0;
        break;
    default:
        *r = 0; *g = 0; *b = 0;
        break;
    }
}

void WS2812Driver::setState(ws2812_state_t state)
{
    if (!s_initialized) {
        return;
    }

    // 모든 LED 상태 업데이트
    for (uint32_t i = 0; i < s_num_leds; i++) {
        s_led_states[i] = state;
    }

    // GRB 데이터 생성
    uint8_t data[s_num_leds * 3];
    for (uint32_t i = 0; i < s_num_leds; i++) {
        uint8_t r, g, b;
        stateToRgb(state, &r, &g, &b);

        // 밝기 적용
        r = (r * s_brightness) / 255;
        g = (g * s_brightness) / 255;
        b = (b * s_brightness) / 255;

        // WS2812B는 GRB 순서
        data[i * 3] = g;
        data[i * 3 + 1] = r;
        data[i * 3 + 2] = b;
    }

    ws2812_hal_transmit(data, sizeof(data));
}

void WS2812Driver::setRgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_initialized) {
        return;
    }

    // 모든 LED 상태를 CUSTOM으로 표시 (저장 안 함)
    uint8_t data[s_num_leds * 3];
    for (uint32_t i = 0; i < s_num_leds; i++) {
        // 밝기 적용
        uint8_t rr = (r * s_brightness) / 255;
        uint8_t gg = (g * s_brightness) / 255;
        uint8_t bb = (b * s_brightness) / 255;

        // WS2812B는 GRB 순서
        data[i * 3] = gg;
        data[i * 3 + 1] = rr;
        data[i * 3 + 2] = bb;
    }

    ws2812_hal_transmit(data, sizeof(data));
}

void WS2812Driver::setLedState(uint32_t led_index, ws2812_state_t state)
{
    if (!s_initialized) {
        return;
    }

    if (led_index >= s_num_leds) {
        T_LOGE("WS2812Drv", "LED 인덱스 초과: %d >= %lu", led_index, s_num_leds);
        return;
    }

    s_led_states[led_index] = state;

    // 전체 LED 데이터 재생성
    uint8_t data[s_num_leds * 3];
    for (uint32_t i = 0; i < s_num_leds; i++) {
        uint8_t r, g, b;
        stateToRgb(s_led_states[i], &r, &g, &b);

        // 밝기 적용
        r = (r * s_brightness) / 255;
        g = (g * s_brightness) / 255;
        b = (b * s_brightness) / 255;

        // WS2812B는 GRB 순서
        data[i * 3] = g;
        data[i * 3 + 1] = r;
        data[i * 3 + 2] = b;
    }

    ws2812_hal_transmit(data, sizeof(data));
}

void WS2812Driver::setLedRgb(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_initialized) {
        return;
    }

    if (led_index >= s_num_leds) {
        T_LOGE("WS2812Drv", "LED 인덱스 초과: %d >= %lu", led_index, s_num_leds);
        return;
    }

    // 밝기 적용
    r = (r * s_brightness) / 255;
    g = (g * s_brightness) / 255;
    b = (b * s_brightness) / 255;

    // WS2812B는 GRB 순서
    uint8_t data[s_num_leds * 3];
    for (uint32_t i = 0; i < s_num_leds; i++) {
        if (i == led_index) {
            data[i * 3] = g;
            data[i * 3 + 1] = r;
            data[i * 3 + 2] = b;
        } else {
            // 다른 LED는 현재 상태 유지
            uint8_t pr, pg, pb;
            stateToRgb(s_led_states[i], &pr, &pg, &pb);
            data[i * 3] = (pg * s_brightness) / 255;
            data[i * 3 + 1] = (pr * s_brightness) / 255;
            data[i * 3 + 2] = (pb * s_brightness) / 255;
        }
    }

    ws2812_hal_transmit(data, sizeof(data));
}

void WS2812Driver::setBrightness(uint8_t brightness)
{
    if (brightness == 0) {
        brightness = 1;
    }

    if (s_brightness != brightness) {
        s_brightness = brightness;
        // 현재 상태 다시 적용
        for (uint32_t i = 0; i < s_num_leds; i++) {
            setLedState(i, s_led_states[i]);
        }
        T_LOGI("WS2812Drv", "밝기 변경: %d", brightness);
    }
}

void WS2812Driver::off(void)
{
    setState(WS2812_OFF);
}

void WS2812Driver::setCameraId(uint8_t camera_id)
{
    if (camera_id == 0) {
        camera_id = 1;  // 최소 1
    }
    s_camera_id = camera_id;
    T_LOGI("WS2812Drv", "카메라 ID 설정: %d", s_camera_id);
}

esp_err_t WS2812Driver::tallyEventCallback(const event_data_t* event)
{
    if (!event || !event->data) {
        return ESP_ERR_INVALID_ARG;
    }

    const tally_event_data_t* tally_data = (const tally_event_data_t*)event->data;

    // 내 카메라 ID의 Tally 상태 확인
    // 이벤트 데이터를 packed_data_t로 변환
    uint8_t data_size = (tally_data->channel_count + 3) / 4;  // 20채널 = 5바이트
    packed_data_t tally = {
        .data = const_cast<uint8_t*>(tally_data->tally_data),
        .data_size = data_size,
        .channel_count = tally_data->channel_count
    };
    uint8_t my_status = packed_data_get_channel(&tally, s_camera_id);

    // Tally 상태 → LED 상태 매핑
    ws2812_state_t led_state;
    const char* state_str;

    switch (my_status) {
    case TALLY_STATUS_PROGRAM:
        led_state = WS2812_PROGRAM;  // 빨강
        state_str = "PROGRAM";
        break;
    case TALLY_STATUS_PREVIEW:
        led_state = WS2812_PREVIEW;  // 초록
        state_str = "PREVIEW";
        break;
    case TALLY_STATUS_BOTH:
        led_state = WS2812_PROGRAM;  // Program 우선 (빨강)
        state_str = "BOTH (PGM+PVW)";
        break;
    default:  // TALLY_STATUS_OFF
        led_state = WS2812_OFF;
        state_str = "OFF";
        break;
    }

    setState(led_state);
    T_LOGI("WS2812Drv", "Tally 이벤트: 카메라 %d → %s", s_camera_id, state_str);

    return ESP_OK;
}

void WS2812Driver::deinit(void)
{
    // 이벤트 구독 취소
    event_bus_unsubscribe(EVT_BRIGHTNESS_CHANGED, brightnessEventCallback);
    event_bus_unsubscribe(EVT_TALLY_STATE_CHANGED, tallyEventCallback);

    ws2812_hal_deinit();
    s_initialized = false;
    T_LOGI("WS2812Drv", "WS2812 드라이버 해제");
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t ws2812_driver_init(int gpio_num, uint32_t num_leds, uint8_t camera_id)
{
    return WS2812Driver::init(gpio_num, num_leds, camera_id);
}

void ws2812_set_state(ws2812_state_t state)
{
    WS2812Driver::setState(state);
}

void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    WS2812Driver::setRgb(r, g, b);
}

void ws2812_set_led_state(uint32_t led_index, ws2812_state_t state)
{
    WS2812Driver::setLedState(led_index, state);
}

void ws2812_set_led_rgb(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    WS2812Driver::setLedRgb(led_index, r, g, b);
}

void ws2812_set_brightness(uint8_t brightness)
{
    WS2812Driver::setBrightness(brightness);
}

void ws2812_set_camera_id(uint8_t camera_id)
{
    WS2812Driver::setCameraId(camera_id);
}

void ws2812_off(void)
{
    WS2812Driver::off();
}

void ws2812_deinit(void)
{
    WS2812Driver::deinit();
}

bool ws2812_is_initialized(void)
{
    return WS2812Driver::isInitialized();
}

}  // extern "C"
