/**
 * @file button_poll.c
 * @brief BUTTON 0 폴링 버튼 감지 구현
 * @author Claude Code
 * @date 2025-12-07
 */

#include "button_poll.h"
#include "button_actions.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "PinConfig.h"
#include <string.h>

static const char* TAG = TAG_BUTTON;

// 폴링 설정
#define POLL_INTERVAL_MS 10      // 10ms 유지 (응답성 유지)
#define DEBOUNCE_MS 20           // 20ms로 증가
#define LONG_PRESS_MS 1000
#define MULTI_CLICK_TIMEOUT_MS 50   // 50ms로 증가

// 버튼 상태
typedef enum {
    BUTTON_STATE_IDLE,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_WAITING_RELEASE
} button_state_t;

// 내부 상태
static struct {
    TaskHandle_t poll_task;
    bool running;
    button_state_t state;
    button_callback_t callback;
    uint64_t press_time;
    uint64_t release_time;
    uint32_t click_count;
    bool long_press_fired;
    bool last_state;
    uint64_t debounce_start;
} s_button = {0};

/**
 * @brief 버튼 상태 초기화
 */
static void reset_button_state(void)
{
    s_button.state = BUTTON_STATE_IDLE;
    s_button.click_count = 0;
    s_button.long_press_fired = false;
    s_button.press_time = 0;
    s_button.release_time = 0;
    // 버튼이 눌린 상태로 유지되는 문제 방지
    s_button.last_state = gpio_get_level(EORA_S3_BUTTON);
}

/**
 * @brief 버튼 액션 처리
 */
static void handle_action(void)
{
    // 1회 클릭만 처리
    button_action_t action = BUTTON_ACTION_SINGLE;

    // button_actions를 통해 기능 실행
    button_actions_execute(action);

    // 기존 콜백은 button_actions_execute에서 이미 처리되므로 중복 호출 불필요
}

/**
 * @brief BUTTON 폴링 태스크
 */
static void button_poll_task(void *arg)
{
    LOG_0(TAG, "BUTTON 폴링 태스크 시작");

    while (s_button.running) {
        uint64_t current_time = esp_timer_get_time();

        // GPIO 상태 읽기 (GPIO 0은 내장 BOOT 버튼, Active Low)
        bool current_state = (gpio_get_level(EORA_S3_BUTTON) == 0);

        // 디바운싱 처리
        if (current_state != s_button.last_state) {
            s_button.debounce_start = current_time;
            s_button.last_state = current_state;
        }

        // 디바운스 시간 경과 후 상태 변경 확인
        if (current_time - s_button.debounce_start >= DEBOUNCE_MS * 1000) {
            if (s_button.state == BUTTON_STATE_IDLE && current_state) {
                // 버튼 눌림
                uint32_t detect_delay = (current_time - s_button.debounce_start) / 1000;
                LOG_0(TAG, "버튼 눌림 (감지 지연: %dms)", detect_delay);
                s_button.state = BUTTON_STATE_PRESSED;
                s_button.press_time = current_time;
                s_button.click_count++;
                s_button.long_press_fired = false;

                // 2회 클릭 이상은 리셋
                if (s_button.click_count > 1) {
                    s_button.click_count = 0;
                    s_button.state = BUTTON_STATE_IDLE;
                    LOG_0(TAG, "다중 클릭 리셋");
                }
            } else if (s_button.state == BUTTON_STATE_PRESSED && !current_state) {
                // 버튼 뗌
                s_button.state = BUTTON_STATE_RELEASED;
                s_button.release_time = current_time;

                uint32_t press_duration = (s_button.release_time - s_button.press_time) / 1000;
                LOG_0(TAG, "버튼 뗌 (%.0fms, 클릭수: %d)", (float)press_duration, s_button.click_count);

                if (!s_button.long_press_fired) {
                    // 1회 클릭은 IDLE 상태로 대기
                    s_button.state = BUTTON_STATE_IDLE;
                }
            } else if (s_button.state == BUTTON_STATE_WAITING_RELEASE && !current_state) {
                // 롱프레스 후 버튼 뗌 - LONG_RELEASE 액션 발생
                LOG_0(TAG, "롱프레스 후 버튼 뗌 - LONG_RELEASE 액션");
                if (s_button.callback) {
                    s_button.callback(BUTTON_ACTION_LONG_RELEASE);
                }
                reset_button_state();
            }
        }

        // 롱 프레스 체크
        if (s_button.state == BUTTON_STATE_PRESSED && !s_button.long_press_fired) {
            if (current_time - s_button.press_time >= LONG_PRESS_MS * 1000) {
                LOG_0(TAG, "롱 프레스! (%.1f초)", LONG_PRESS_MS / 1000.0);
                s_button.long_press_fired = true;
                s_button.click_count = 0;

                if (s_button.callback) {
                    s_button.callback(BUTTON_ACTION_LONG);
                }

                // 롱프레스 후 상태를 대기 상태로 변경 (버튼을 뗄 때까지 대기)
                s_button.state = BUTTON_STATE_WAITING_RELEASE;
            }
        }

        // 멀티클릭 타임아웃 체크
        if (s_button.state == BUTTON_STATE_IDLE && s_button.click_count > 0) {
            if (current_time - s_button.release_time >= MULTI_CLICK_TIMEOUT_MS * 1000) {
                // 타임아웃: 액션 처리
                handle_action();
                reset_button_state();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }

    LOG_0(TAG, "BUTTON 폴링 태스크 종료");
    vTaskDelete(NULL);
}

esp_err_t button_poll_init(void)
{
    // 버튼 액션 초기화
    button_actions_init();

    // 콜백 설정 (button_actions를 통해 처리)
    s_button.callback = button_actions_execute;

    // GPIO 설정
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EORA_S3_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        LOG_0(TAG, "GPIO 설정 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 초기 상태 읽기
    s_button.last_state = (gpio_get_level(EORA_S3_BUTTON) == 0);
    s_button.debounce_start = esp_timer_get_time();

    LOG_0(TAG, "GPIO 0 폴링 초기화 완료");
    return ESP_OK;
}

esp_err_t button_poll_start(void)
{
    if (s_button.running) {
        return ESP_OK;
    }

    reset_button_state();
    s_button.running = true;

    BaseType_t ret = xTaskCreate(
        button_poll_task,
        "button_poll",
        8192,  // 스택 크기 더 증가 (4096 -> 8192)
        NULL,
        5,
        &s_button.poll_task
    );

    if (ret != pdPASS) {
        LOG_0(TAG, "폴링 태스크 생성 실패");
        s_button.running = false;
        return ESP_ERR_NO_MEM;
    }

    LOG_0(TAG, "GPIO 폴링 태스크 시작 완료");
    return ESP_OK;
}

void button_poll_stop(void)
{
    s_button.running = false;

    if (s_button.poll_task) {
        vTaskDelete(s_button.poll_task);
        s_button.poll_task = NULL;
    }

    LOG_0(TAG, "GPIO 폴링 중지");
}

void button_poll_set_callback(button_callback_t callback)
{
    s_button.callback = callback;
}