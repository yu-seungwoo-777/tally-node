#include "log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief 로그 시스템 테스트 함수
 */
void test_log_system(void) {
    // 로그 시스템 초기화 (기본 레벨 0)
    log_init(LOG_LEVEL_0);

    // 레벨 0 로그 출력 (항상 출력됨)
    LOG_0("TEST", "This is level 0 log - always shown");

    // 레벨 1 로그 출력 (레벨 0이므로 출력 안 됨)
    LOG_1("TEST", "This is level 1 log - should NOT be shown");

    // 로그 레벨을 1로 변경
    log_set_level(LOG_LEVEL_1);
    LOG_0("TEST", "Changed log level to 1");

    // 이제 레벨 1 로그도 출력됨
    LOG_1("TEST", "This is level 1 log - should be shown now");

    // 현재 로그 레벨 확인
    log_level_t current_level = log_get_level();
    LOG_0("TEST", "Current log level: %d", current_level);

    // 로그 버퍼 비우기
    log_flush();

    LOG_0("TEST", "Log system test completed");
}

/**
 * @brief 로그 시스템 스트레스 테스트
 */
void test_log_stress(void) {
    log_init(LOG_LEVEL_1);

    LOG_0("STRESS", "Starting stress test...");

    // 100개의 로그 메시지 출력
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            LOG_0("STRESS", "Message %d (level 0)", i);
        } else {
            LOG_1("STRESS", "Message %d (level 1)", i);
        }

        // 약간의 지연
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    LOG_0("STRESS", "Stress test completed");
    log_flush();
}
