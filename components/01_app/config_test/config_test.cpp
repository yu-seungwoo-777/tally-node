/**
 * @file config_test.cpp
 * @brief ConfigService 테스트 앱 구현
 *
 * ConfigService 조회 기능 테스트:
 * - device_id (MAC 뒤 4자리)
 * - device 설정 (brightness, camera_id, rf)
 * - system 상태 (battery, uptime, stopped)
 */

#include "config_test.h"
#include "ConfigService.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ConfigTest";

// ============================================================================
// 테스트 함수
// ============================================================================

/**
 * @brief Device ID 테스트
 */
static void test_device_id(void)
{
    T_LOGI(TAG, "=== Device ID 테스트 ===");

    const char* device_id = config_service_get_device_id();

    T_LOGI(TAG, "  Device ID: %s", device_id);
}

/**
 * @brief Device 설정 테스트
 */
static void test_device_config(void)
{
    T_LOGI(TAG, "=== Device 설정 테스트 ===");

    config_device_t dev;
    esp_err_t ret = config_service_get_device(&dev);

    if (ret == ESP_OK) {
        T_LOGI(TAG, "  Brightness: %d", dev.brightness);
        T_LOGI(TAG, "  Camera ID: %d", dev.camera_id);
        T_LOGI(TAG, "  RF Frequency: %.1f MHz", dev.rf.frequency);
        T_LOGI(TAG, "  RF Sync Word: 0x%02X", dev.rf.sync_word);
    } else {
        T_LOGE(TAG, "  Device 설정 로드 실패: %d", ret);
    }
}

/**
 * @brief System 상태 테스트
 */
static void test_system_status(void)
{
    T_LOGI(TAG, "=== System 상태 테스트 ===");

    config_system_t sys;
    config_service_get_system(&sys);

    T_LOGI(TAG, "  Device ID: %s", sys.device_id);
    T_LOGI(TAG, "  Battery: %d%%", sys.battery);
    T_LOGI(TAG, "  Uptime: %lu sec", sys.uptime);
    T_LOGI(TAG, "  Stopped: %s", sys.stopped ? "true" : "false");
}

// ============================================================================
// 공개 API
// ============================================================================

esp_err_t config_test_app_init(void)
{
    T_LOGI(TAG, "ConfigService 테스트 앱 초기화 중...");

    // ConfigService 초기화
    esp_err_t ret = config_service_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "ConfigService 초기화 실패: %d", ret);
        return ret;
    }

    T_LOGI(TAG, "");
    T_LOGI(TAG, "========== ConfigService 조회 테스트 ==========");

    // 테스트 실행
    test_device_id();
    T_LOGI(TAG, "");

    test_device_config();
    T_LOGI(TAG, "");

    test_system_status();
    T_LOGI(TAG, "");

    T_LOGI(TAG, "===========================================");
    T_LOGI(TAG, "");
    T_LOGI(TAG, "초당 배터리/업타임 표시 시작...");

    return ESP_OK;
}

void config_test_app_stop(void)
{
    T_LOGI(TAG, "ConfigService 테스트 앱 정지");
}

void config_test_app_deinit(void)
{
    T_LOGI(TAG, "ConfigService 테스트 앱 해제");
}

/**
 * @brief 1초마다 호출 - 배터리/업타임 표시
 */
void config_test_app_tick(void)
{
    // 배터리 업데이트
    uint8_t battery = config_service_update_battery();

    // 업타임 증가
    config_service_inc_uptime();

    // 시스템 상태 가져오기
    config_system_t sys;
    config_service_get_system(&sys);

    // 표시
    T_LOGI(TAG, "Battery: %d%% | Uptime: %lu sec", battery, sys.uptime);
}
