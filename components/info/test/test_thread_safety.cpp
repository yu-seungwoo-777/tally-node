/**
 * @file test_thread_safety.cpp
 * @brief InfoManager 스레드 안전성 테스트
 */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "info/info_manager.h"

// 테스트 설정
#define NUM_TASKS 5
#define NUM_ITERATIONS 100
#define TEST_TIMEOUT_MS 10000

// 테스트 결과
static volatile uint32_t g_success_count = 0;
static volatile uint32_t g_error_count = 0;
static volatile bool g_test_complete = false;
static SemaphoreHandle_t g_test_mutex = NULL;

// 작업 함수
static void test_task(void* arg)
{
    uint32_t task_id = (uint32_t)arg;
    char device_id[INFO_DEVICE_ID_MAX_LEN];
    info_system_info_t info;

    LOG_0(TAG_TEST, "Task %u 시작", task_id);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // 장치 ID 읽기
        if (info_manager_get_device_id(device_id, sizeof(device_id)) == ESP_OK) {
            // 유효성 검사
            if (strlen(device_id) > 0 && strlen(device_id) < INFO_DEVICE_ID_MAX_LEN) {
                // 성공
                xSemaphoreTake(g_test_mutex, portMAX_DELAY);
                g_success_count++;
                xSemaphoreGive(g_test_mutex);
            } else {
                // 실패
                xSemaphoreTake(g_test_mutex, portMAX_DELAY);
                g_error_count++;
                xSemaphoreGive(g_test_mutex);
            }
        } else {
            xSemaphoreTake(g_test_mutex, portMAX_DELAY);
            g_error_count++;
            xSemaphoreGive(g_test_mutex);
        }

        // 시스템 정보 읽기
        if (info_manager_get_system_info(&info) == ESP_OK) {
            // 기본값 확인
            if (info.uptime_sec > 0 && info.battery_percent >= 0 && info.battery_percent <= 100) {
                // 유효
            } else {
                xSemaphoreTake(g_test_mutex, portMAX_DELAY);
                g_error_count++;
                xSemaphoreGive(g_test_mutex);
            }
        }

        // 짧은 지연
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    LOG_0(TAG_TEST, "Task %u 완료", task_id);
    vTaskDelete(NULL);
}

// 테스트 케이스
TEST_CASE("InfoManager 스레드 안전성", "[info][multithread]")
{
    // 초기화
    g_success_count = 0;
    g_error_count = 0;
    g_test_complete = false;

    g_test_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(g_test_mutex);

    // InfoManager 초기화 확인
    TEST_ASSERT_TRUE(info_manager_is_initialized());

    // 태스크 생성
    TaskHandle_t handles[NUM_TASKS];
    for (int i = 0; i < NUM_TASKS; i++) {
        BaseType_t ret = xTaskCreate(
            test_task,
            "test_task",
            4096,
            (void*)i,
            uxTaskPriorityGet(NULL),
            &handles[i]
        );
        TEST_ASSERT_EQUAL(pdPASS, ret);
    }

    // 타임아웃 대기
    uint64_t start_time = esp_timer_get_time();
    while (!g_test_complete) {
        vTaskDelay(pdMS_TO_TICKS(100));

        // 모든 태스크가 완료되었는지 확인
        bool all_done = true;
        for (int i = 0; i < NUM_TASKS; i++) {
            if (eTaskGetState(handles[i]) != eDeleted) {
                all_done = false;
                break;
            }
        }

        if (all_done) {
            break;
        }

        // 타임아웃 체크
        if ((esp_timer_get_time() - start_time) > TEST_TIMEOUT_MS * 1000) {
            LOG_0(TAG_TEST, "테스트 타임아웃!");
            break;
        }
    }

    // 결과 확인
    uint32_t expected_ops = NUM_TASKS * NUM_ITERATIONS;
    LOG_0(TAG_TEST, "성공: %u, 실패: %u, 총: %u",
          g_success_count, g_error_count, expected_ops);

    // 모든 작업이 성공해야 함
    TEST_ASSERT_EQUAL(expected_ops, g_success_count + g_error_count);
    TEST_ASSERT_EQUAL(0, g_error_count);

    // 정리
    vSemaphoreDelete(g_test_mutex);
}

// 장치 ID 변경 테스트
static void modify_task(void* arg)
{
    (void)arg;

    for (int i = 0; i < 10; i++) {
        char new_id[16];
        snprintf(new_id, sizeof(new_id), "T%u", i);

        if (info_manager_set_device_id(new_id) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));

            char verify_id[INFO_DEVICE_ID_MAX_LEN];
            if (info_manager_get_device_id(verify_id, sizeof(verify_id)) == ESP_OK) {
                if (strcmp(verify_id, new_id) == 0) {
                    xSemaphoreTake(g_test_mutex, portMAX_DELAY);
                    g_success_count++;
                    xSemaphoreGive(g_test_mutex);
                } else {
                    xSemaphoreTake(g_test_mutex, portMAX_DELAY);
                    g_error_count++;
                    xSemaphoreGive(g_test_mutex);
                }
            }
        }
    }

    vTaskDelete(NULL);
}

TEST_CASE("InfoManager 동시 수정 테스트", "[info][multithread]")
{
    // 초기화
    g_success_count = 0;
    g_error_count = 0;

    g_test_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(g_test_mutex);

    // 읽기 태스크 생성
    TaskHandle_t read_handle;
    xTaskCreate(modify_task, "modify_task", 4096, NULL, 2, &read_handle);

    // 읽기 태스크 생성 (여러 개)
    TaskHandle_t read_handles[3];
    for (int i = 0; i < 3; i++) {
        xTaskCreate(
            test_task,
            "read_task",
            4096,
            (void*)i,
            1,
            &read_handles[i]
        );
    }

    // 대기
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 결과 확인
    LOG_0(TAG_TEST, "수정 테스트 - 성공: %u, 실패: %u",
          g_success_count, g_error_count);

    // 실패가 없어야 함 (스레드 안전성)
    TEST_ASSERT_EQUAL(0, g_error_count);
    TEST_ASSERT_GREATER_THAN(0, g_success_count);

    // 정리
    vSemaphoreDelete(g_test_mutex);
}