/**
 * @file ButtonService.cpp
 * @brief 버튼 서비스 구현
 */

#include "driver/gpio.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "button_service.h"
#include "PinConfig.h"
#include "event_bus.h"
#include "t_log.h"

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

/**
 * @brief 버튼 내부 상태
 */
typedef enum {
    BUTTON_STATE_IDLE,            /**< 대기 중 */
    BUTTON_STATE_PRESSED,          /**< 눌림 */
    BUTTON_STATE_RELEASED,         /**< 해제됨 */
    BUTTON_STATE_WAITING_RELEASE   /**< 롱프레스 후 해제 대기 */
} button_state_t;

// ============================================================================
// ButtonService 클래스
// ============================================================================

class ButtonService {
public:
    static esp_err_t init(void);
    static void start(void);
    static void stop(void);
    static void deinit(void);
    static bool isPressed(void);
    static bool isInitialized(void) { return s_initialized; }

private:
    ButtonService() = delete;
    ~ButtonService() = delete;

    static void resetState(void);
    static void button_task(void* arg);

    static bool s_initialized;
    static bool s_running;
    static TaskHandle_t s_task;

    // 버튼 상태
    static button_state_t s_state;
    static uint64_t s_press_time;        /**< 눌린 시간 (us) */
    static uint64_t s_release_time;      /**< 떼어진 시간 (us) */
    static uint64_t s_debounce_start;    /**< 디바운스 시작 시간 (us) */
    static uint64_t s_last_repeat_time;  /**< 마지막 반복 이벤트 시간 (us) */
    static uint32_t s_click_count;       /**< 클릭 횟수 */
    static bool s_long_press_fired;      /**< 롱 프레스 발생 플래그 */
    static bool s_last_gpio_state;       /**< 이전 GPIO 상태 */
};

// ============================================================================
// 정적 변수 초기화
// ============================================================================

bool ButtonService::s_initialized = false;
bool ButtonService::s_running = false;
TaskHandle_t ButtonService::s_task = nullptr;

button_state_t ButtonService::s_state = BUTTON_STATE_IDLE;
uint64_t ButtonService::s_press_time = 0;
uint64_t ButtonService::s_release_time = 0;
uint64_t ButtonService::s_debounce_start = 0;
uint64_t ButtonService::s_last_repeat_time = 0;
uint32_t ButtonService::s_click_count = 0;
bool ButtonService::s_long_press_fired = false;
bool ButtonService::s_last_gpio_state = false;

// ============================================================================
// 내부 함수
// ============================================================================

void ButtonService::resetState(void)
{
    s_state = BUTTON_STATE_IDLE;
    s_click_count = 0;
    s_long_press_fired = false;
    s_press_time = 0;
    s_release_time = 0;
    s_last_repeat_time = 0;
    s_last_gpio_state = (gpio_get_level(EORA_S3_BUTTON) == 0);
    s_debounce_start = esp_timer_get_time();
}

void ButtonService::button_task(void* arg)
{
    (void)arg;

    T_LOGI(TAG, "버튼 폴링 태스크 시작");

    while (s_running) {
        uint64_t current_time = esp_timer_get_time();

        // GPIO 상태 읽기 (Active Low: 누르면 0)
        bool current_state = (gpio_get_level(EORA_S3_BUTTON) == 0);

        // 디바운싱 처리
        if (current_state != s_last_gpio_state) {
            s_debounce_start = current_time;
            s_last_gpio_state = current_state;
        }

        // 디바운스 시간 경과 후 상태 변경 처리
        if (current_time - s_debounce_start >= DEBOUNCE_MS * 1000ULL) {
            if (s_state == BUTTON_STATE_IDLE && current_state) {
                // 버튼 눌림 감지
                s_state = BUTTON_STATE_PRESSED;
                s_press_time = current_time;
                s_click_count++;
                s_long_press_fired = false;

                // 2회 클릭 이상은 리셋 (더블 클릭 미지원)
                if (s_click_count > 1) {
                    s_click_count = 1;
                }

            } else if (s_state == BUTTON_STATE_PRESSED && !current_state) {
                // 버튼 떼어짐 감지
                s_state = BUTTON_STATE_RELEASED;
                s_release_time = current_time;

                if (!s_long_press_fired) {
                    // 롱 프레스가 아니면 IDLE로 전환
                    s_state = BUTTON_STATE_IDLE;
                }

            } else if (s_state == BUTTON_STATE_WAITING_RELEASE && !current_state) {
                // 롱 프레스 후 버튼 떼어짐
                T_LOGI(TAG, "롱 프레스 해제");
                event_bus_publish(EVT_BUTTON_LONG_RELEASE, nullptr, 0);
                resetState();
            }
        }

        // 롱 프레스 체크
        if (s_state == BUTTON_STATE_PRESSED && !s_long_press_fired) {
            uint64_t press_duration = current_time - s_press_time;
            if (press_duration >= LONG_PRESS_MS * 1000ULL) {
                // 롱 프레스 시작
                T_LOGI(TAG, "롱 프레스 시작 (%.1f초)", LONG_PRESS_MS / 1000.0f);
                s_long_press_fired = true;
                s_click_count = 0;
                s_state = BUTTON_STATE_WAITING_RELEASE;
                s_last_repeat_time = current_time;

                event_bus_publish(EVT_BUTTON_LONG_PRESS, nullptr, 0);
            }
        }

        // 롱 프레스 유지 중 반복 이벤트 발행
        if (s_state == BUTTON_STATE_WAITING_RELEASE && s_long_press_fired && current_state) {
            uint64_t repeat_elapsed = current_time - s_last_repeat_time;
            if (repeat_elapsed >= LONG_PRESS_REPEAT_MS * 1000ULL) {
                s_last_repeat_time = current_time;
                event_bus_publish(EVT_BUTTON_LONG_PRESS, nullptr, 0);
            }
        }

        // 멀티클릭 타임아웃 체크 (단일 클릭 확인)
        if (s_state == BUTTON_STATE_IDLE && s_click_count > 0) {
            uint64_t idle_time = current_time - s_release_time;
            if (idle_time >= MULTI_CLICK_TIMEOUT_MS * 1000ULL) {
                // 단일 클릭 확정
                T_LOGV(TAG, "단일 클릭");
                event_bus_publish(EVT_BUTTON_SINGLE_CLICK, nullptr, 0);
                resetState();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }

    T_LOGI(TAG, "버튼 폴링 태스크 종료");
    vTaskDelete(nullptr);
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t ButtonService::init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // GPIO 설정
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EORA_S3_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // 폴링 모드이므로 인터럽트 비활성화
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "GPIO 설정 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 초기 상태 읽기
    resetState();

    s_initialized = true;
    T_LOGI(TAG, "버튼 서비스 초기화 완료 (GPIO %d)", EORA_S3_BUTTON);
    return ESP_OK;
}

void ButtonService::start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return;
    }

    if (s_running) {
        T_LOGW(TAG, "이미 실행 중");
        return;
    }

    resetState();
    s_running = true;

    BaseType_t ret = xTaskCreate(
        button_task,
        "button_task",
        4096,
        nullptr,
        5,  // 우선순위
        &s_task
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "폴링 태스크 생성 실패");
        s_running = false;
        return;
    }

    T_LOGI(TAG, "버튼 서비스 시작");
}

void ButtonService::stop(void)
{
    if (!s_running) {
        return;
    }

    s_running = false;

    // 태스크 종료 대기
    if (s_task != nullptr) {
        int wait_count = 0;
        while (eTaskGetState(s_task) != eDeleted && wait_count < 20) {
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
        }
        s_task = nullptr;
    }

    T_LOGI(TAG, "버튼 서비스 정지");
}

void ButtonService::deinit(void)
{
    stop();
    s_initialized = false;
    T_LOGI(TAG, "버튼 서비스 해제 완료");
}

bool ButtonService::isPressed(void)
{
    return (gpio_get_level(EORA_S3_BUTTON) == 0);
}

// ============================================================================
// C 인터페이스 (extern "C")
// ============================================================================

extern "C" {

esp_err_t button_service_init(void)
{
    return ButtonService::init();
}

esp_err_t button_service_start(void)
{
    ButtonService::start();
    return ESP_OK;
}

void button_service_stop(void)
{
    ButtonService::stop();
}

void button_service_deinit(void)
{
    ButtonService::deinit();
}

bool button_service_is_pressed(void)
{
    return ButtonService::isPressed();
}

bool button_service_is_initialized(void)
{
    return ButtonService::isInitialized();
}

}  // extern "C"
