/**
 * @file button_poll.c
 * @brief 버튼 폴링 컴포넌트 구현
 *
 * EoRa-S3 내장 버튼 (GPIO 0) 폴링
 * - Active Low (누르면 0)
 * - 10ms 폴링 주기
 * - 20ms 디바운싱
 * - 1000ms 롱 프레스
 */

#include "button_poll.h"
#include "PinConfig.h"
#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const char* TAG = "BUTTON";

// ============================================================================
// 설정
// ============================================================================

#define POLL_INTERVAL_MS     10    /**< 폴링 주기 (ms) */
#define DEBOUNCE_MS          20    /**< 디바운스 시간 (ms) */
#define LONG_PRESS_MS        1000  /**< 롱 프레스 시간 (ms) */
#define LONG_PRESS_REPEAT_MS 500   /**< 롱 프레스 반복 간격 (ms) */
#define MULTI_CLICK_TIMEOUT_MS 50  /**< 멀티클릭 타임아웃 (ms) */

// ============================================================================
// 버튼 상태
// ============================================================================

typedef enum {
    BUTTON_STATE_IDLE,            /**< 대기 중 */
    BUTTON_STATE_PRESSED,          /**< 눌림 */
    BUTTON_STATE_RELEASED,         /**< 해제됨 */
    BUTTON_STATE_WAITING_RELEASE   /**< 롱프레스 후 해제 대기 */
} button_state_t;

// ============================================================================
// 내부 상태 구조체
// ============================================================================

typedef struct {
    TaskHandle_t poll_task;        /**< 폴링 태스크 핸들 */
    bool running;                  /**< 실행 중 플래그 */
    button_state_t state;          /**< 현재 상태 */
    button_callback_t callback;    /**< 콜백 함수 */

    uint64_t press_time;           /**< 눌린 시간 (us) */
    uint64_t release_time;         /**< 떼어진 시간 (us) */
    uint64_t debounce_start;       /**< 디바운스 시작 시간 (us) */
    uint64_t last_repeat_time;     /**< 마지막 반복 이벤트 시간 (us) */

    uint32_t click_count;          /**< 클릭 횟수 */
    bool long_press_fired;         /**< 롱 프레스 발생 플래그 */
    bool last_state;               /**< 이전 GPIO 상태 */
} button_context_t;

// ============================================================================
// 정적 변수
// ============================================================================

static button_context_t s_btn = {0};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 버튼 상태 초기화
 */
static void reset_button_state(void)
{
    s_btn.state = BUTTON_STATE_IDLE;
    s_btn.click_count = 0;
    s_btn.long_press_fired = false;
    s_btn.press_time = 0;
    s_btn.release_time = 0;
    s_btn.last_repeat_time = 0;
    s_btn.last_state = (gpio_get_level(EORA_S3_BUTTON) == 0);
    s_btn.debounce_start = esp_timer_get_time();
}

/**
 * @brief 버튼 콜백 호출 (안전 래퍼)
 */
static void invoke_callback(button_action_t action)
{
    if (s_btn.callback) {
        s_btn.callback(action);
    }
}

// ============================================================================
// 폴링 태스크
// ============================================================================

/**
 * @brief 버튼 폴링 태스크
 *
 * 10ms마다 GPIO 상태를 확인하여 버튼 이벤트를 감지합니다.
 */
static void button_poll_task(void* arg)
{
    T_LOGI(TAG, "버튼 폴링 태스크 시작");

    while (s_btn.running) {
        uint64_t current_time = esp_timer_get_time();

        // GPIO 상태 읽기 (Active Low: 누르면 0)
        bool current_state = (gpio_get_level(EORA_S3_BUTTON) == 0);

        // 디바운싱 처리
        if (current_state != s_btn.last_state) {
            s_btn.debounce_start = current_time;
            s_btn.last_state = current_state;
        }

        // 디바운스 시간 경과 후 상태 변경 처리
        if (current_time - s_btn.debounce_start >= DEBOUNCE_MS * 1000ULL) {
            if (s_btn.state == BUTTON_STATE_IDLE && current_state) {
                // 버튼 눌림 감지
                s_btn.state = BUTTON_STATE_PRESSED;
                s_btn.press_time = current_time;
                s_btn.click_count++;
                s_btn.long_press_fired = false;

                // 2회 클릭 이상은 리셋 (더블 클릭 미지원)
                if (s_btn.click_count > 1) {
                    s_btn.click_count = 1;
                }

            } else if (s_btn.state == BUTTON_STATE_PRESSED && !current_state) {
                // 버튼 떼어짐 감지
                s_btn.state = BUTTON_STATE_RELEASED;
                s_btn.release_time = current_time;

                if (!s_btn.long_press_fired) {
                    // 롱 프레스가 아니면 IDLE로 전환
                    s_btn.state = BUTTON_STATE_IDLE;
                }

            } else if (s_btn.state == BUTTON_STATE_WAITING_RELEASE && !current_state) {
                // 롱 프레스 후 버튼 떼어짐
                T_LOGI(TAG, "롱 프레스 해제");
                invoke_callback(BUTTON_ACTION_LONG_RELEASE);
                event_bus_publish(EVT_BUTTON_LONG_RELEASE, NULL, 0);
                reset_button_state();
            }
        }

        // 롱 프레스 체크
        if (s_btn.state == BUTTON_STATE_PRESSED && !s_btn.long_press_fired) {
            uint64_t press_duration = current_time - s_btn.press_time;
            if (press_duration >= LONG_PRESS_MS * 1000ULL) {
                // 롱 프레스 시작
                T_LOGI(TAG, "롱 프레스 시작 (%.1f초)", LONG_PRESS_MS / 1000.0f);
                s_btn.long_press_fired = true;
                s_btn.click_count = 0;
                s_btn.state = BUTTON_STATE_WAITING_RELEASE;
                s_btn.last_repeat_time = current_time;

                invoke_callback(BUTTON_ACTION_LONG);
                event_bus_publish(EVT_BUTTON_LONG_PRESS, NULL, 0);
            }
        }

        // 롱 프레스 유지 중 반복 이벤트 발행
        if (s_btn.state == BUTTON_STATE_WAITING_RELEASE && s_btn.long_press_fired && current_state) {
            uint64_t repeat_elapsed = current_time - s_btn.last_repeat_time;
            if (repeat_elapsed >= LONG_PRESS_REPEAT_MS * 1000ULL) {
                s_btn.last_repeat_time = current_time;
                event_bus_publish(EVT_BUTTON_LONG_PRESS, NULL, 0);
            }
        }

        // 멀티클릭 타임아웃 체크 (단일 클릭 확인)
        if (s_btn.state == BUTTON_STATE_IDLE && s_btn.click_count > 0) {
            uint64_t idle_time = current_time - s_btn.release_time;
            if (idle_time >= MULTI_CLICK_TIMEOUT_MS * 1000ULL) {
                // 단일 클릭 확정
                T_LOGV(TAG, "단일 클릭");
                invoke_callback(BUTTON_ACTION_SINGLE);
                reset_button_state();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }

    T_LOGI(TAG, "버튼 폴링 태스크 종료");
    vTaskDelete(NULL);
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t button_poll_init(void)
{
    // 이미 초기화됨
    if (s_btn.poll_task != NULL) {
        T_LOGI(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // GPIO 설정
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EORA_S3_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  /**< 폴링 모드이므로 인터럽트 비활성화 */
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        T_LOGI(TAG, "GPIO 설정 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 초기 상태 읽기
    reset_button_state();

    T_LOGI(TAG, "버튼 폴링 초기화 완료 (GPIO %d)", EORA_S3_BUTTON);
    return ESP_OK;
}

esp_err_t button_poll_start(void)
{
    if (s_btn.running) {
        T_LOGI(TAG, "이미 실행 중");
        return ESP_OK;
    }

    reset_button_state();
    s_btn.running = true;

    BaseType_t ret = xTaskCreate(
        button_poll_task,
        "button_poll",
        4096,
        NULL,
        5,  // 우선순위
        &s_btn.poll_task
    );

    if (ret != pdPASS) {
        T_LOGI(TAG, "폴링 태스크 생성 실패");
        s_btn.running = false;
        return ESP_ERR_NO_MEM;
    }

    T_LOGI(TAG, "버튼 폴링 시작");
    return ESP_OK;
}

void button_poll_stop(void)
{
    if (!s_btn.running) {
        return;
    }

    s_btn.running = false;

    // 태스크 종료 대기
    if (s_btn.poll_task) {
        int wait_count = 0;
        while (eTaskGetState(s_btn.poll_task) != eDeleted && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
        }
        s_btn.poll_task = NULL;
    }

    T_LOGI(TAG, "버튼 폴링 정지");
}

void button_poll_deinit(void)
{
    button_poll_stop();
    s_btn.callback = NULL;
    T_LOGI(TAG, "버튼 폴링 해제 완료");
}

void button_poll_set_callback(button_callback_t callback)
{
    s_btn.callback = callback;
}

bool button_poll_is_pressed(void)
{
    return (gpio_get_level(EORA_S3_BUTTON) == 0);
}
