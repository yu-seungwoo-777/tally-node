/**
 * @file tally_test_service.cpp
 * @brief Tally 테스트 모드 서비스 구현
 *
 * PGM/PVW 패턴을 순환하며 Tally 데이터를 전송하는 테스트 모드
 * 패턴 (4채널 기준):
 *   - PGM1, PVW2
 *   - PGM2, PVW3
 *   - PGM3, PVW4
 *   - PGM4, PVW1
 *   - 반복...
 */

#include "tally_test_service.h"
#include "event_bus.h"
#include "t_log.h"
#include "system_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "03_TallyTest";

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    bool initialized;
    bool running;
    uint8_t max_channels;
    uint16_t interval_ms;
    TaskHandle_t task_handle;
} s_test = {
    .initialized = false,
    .running = false,
    .max_channels = 4,
    .interval_ms = 500,
    .task_handle = nullptr
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 테스트 패턴 생성
 *
 * 패턴 (max_channels=4 기준):
 *   step=0: PGM1, PVW2
 *   step=1: PGM2, PVW3
 *   step=2: PGM3, PVW4
 *   step=3: PGM4, PVW1
 */
static void generate_test_pattern(uint8_t step, uint8_t* pgm_channels,
                                   uint8_t* pgm_count, uint8_t* pvw_channels,
                                   uint8_t* pvw_count)
{
    uint8_t max = s_test.max_channels;

    // PGM: step+1 (1~max 순환)
    *pgm_count = 1;
    pgm_channels[0] = (step % max) + 1;

    // PVW: step+2 (1~max 순환)
    *pvw_count = 1;
    pvw_channels[0] = ((step + 1) % max) + 1;
}

/**
 * @brief Tally 이벤트 발행
 */
static void publish_tally_event(uint8_t step)
{
    uint8_t pgm_channels[20] = {0};
    uint8_t pvw_channels[20] = {0};
    uint8_t pgm_count = 0;
    uint8_t pvw_count = 0;

    generate_test_pattern(step, pgm_channels, &pgm_count, pvw_channels, &pvw_count);

    // packed_data_t 생성
    // 최대 20채널 = 5바이트 (2비트/채널)
    uint8_t tally_data[8] = {0};

    // PGM 채널 설정 (비트 0=1)
    for (uint8_t i = 0; i < pgm_count && i < 20; i++) {
        uint8_t ch = pgm_channels[i] - 1;  // 0-based
        if (ch < 20) {
            tally_data[ch / 4] |= (1 << ((ch % 4) * 2));
        }
    }

    // PVW 채널 설정 (비트 1=2)
    for (uint8_t i = 0; i < pvw_count && i < 20; i++) {
        uint8_t ch = pvw_channels[i] - 1;  // 0-based
        if (ch < 20) {
            tally_data[ch / 4] |= (2 << ((ch % 4) * 2));
        }
    }

    // 이벤트 발행
    static tally_event_data_t s_tally_event;
    s_tally_event.source = 0;  // Test mode
    s_tally_event.channel_count = s_test.max_channels;
    s_tally_event.tally_value = 0;

    // little-endian 64비트 변환
    for (int i = 0; i < 8; i++) {
        s_tally_event.tally_data[i] = tally_data[i];
        s_tally_event.tally_value |= ((uint64_t)tally_data[i] << (i * 8));
    }

    event_bus_publish(EVT_TALLY_STATE_CHANGED, &s_tally_event, sizeof(s_tally_event));

    T_LOGI(TAG, "Test step %d: PGM%d PVW%d", step, pgm_channels[0], pvw_channels[0]);
}

/**
 * @brief 테스트 모드 태스크
 */
static void test_mode_task(void* arg)
{
    (void)arg;
    uint8_t step = 0;

    T_LOGI(TAG, "Test mode started: channels=%d, interval=%dms",
             s_test.max_channels, s_test.interval_ms);

    // WDT에 태스크 등록
    system_wdt_register_task("tally_test_task");

    while (s_test.running) {
        // WDT 리셋 (루프마다)
        system_wdt_reset();

        publish_tally_event(step);

        step++;
        if (step >= s_test.max_channels) {
            step = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(s_test.interval_ms));
    }

    // WDT에서 태스크 제거
    system_wdt_unregister_task();

    T_LOGI(TAG, "Test mode stopped");
    vTaskDelete(nullptr);
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t tally_test_service_init(void)
{
    if (s_test.initialized) {
        return ESP_OK;
    }

    memset(&s_test, 0, sizeof(s_test));
    s_test.initialized = true;

    T_LOGI(TAG, "Tally test service initialized");
    return ESP_OK;
}

esp_err_t tally_test_service_start(uint8_t max_channels, uint16_t interval_ms)
{
    if (!s_test.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_test.running) {
        T_LOGW(TAG, "Test mode already running");
        return ESP_OK;
    }

    // 이전 태스크 핸들 정리 (남아있는 경우)
    if (s_test.task_handle) {
        TaskHandle_t old_handle = s_test.task_handle;
        eTaskState old_state = eTaskGetState(old_handle);
        if (old_state == eDeleted || old_state == eInvalid) {
            T_LOGW(TAG, "Cleaning up stale task handle");
            s_test.task_handle = nullptr;
        } else {
            T_LOGW(TAG, "Previous task still exists (state=%d), force cleanup", old_state);
            s_test.task_handle = nullptr;
        }
    }

    // 파라미터 검증
    if (max_channels < 1 || max_channels > 20) {
        T_LOGE(TAG, "Invalid max_channels: %d (must be 1-20)", max_channels);
        return ESP_ERR_INVALID_ARG;
    }

    if (interval_ms < 100 || interval_ms > 3000) {
        T_LOGE(TAG, "Invalid interval_ms: %d (must be 100-3000)", interval_ms);
        return ESP_ERR_INVALID_ARG;
    }

    s_test.max_channels = max_channels;
    s_test.interval_ms = interval_ms;
    s_test.running = true;

    // 태스크 생성
    BaseType_t ret = xTaskCreatePinnedToCore(
        test_mode_task,
        "tally_test",
        4096,
        nullptr,
        5,  // 우선순위 (낮음)
        &s_test.task_handle,
        1   // Core 1
    );

    if (ret != pdPASS) {
        s_test.running = false;
        s_test.task_handle = nullptr;
        T_LOGE(TAG, "Failed to create test mode task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void tally_test_service_stop(void)
{
    if (!s_test.running) {
        return;
    }

    s_test.running = false;

    // 태스크 종료 대기 (최대 5초)
    if (s_test.task_handle) {
        int wait_count = 0;
        while (eTaskGetState(s_test.task_handle) != eDeleted && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }
        if (wait_count >= 50) {
            T_LOGW(TAG, "Test mode task deletion timeout");
        }
        s_test.task_handle = nullptr;
    }

    T_LOGI(TAG, "Test mode stop completed");
}

bool tally_test_service_is_running(void)
{
    return s_test.running;
}

} // extern "C"
