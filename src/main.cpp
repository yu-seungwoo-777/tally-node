/**
 * @file main.cpp
 * @brief EoRa-S3 메인
 *
 * 01_app 계층 - 애플리케이션 진입점
 * - NVS 초기화
 * - 앱 실행
 *
 * 앱 선택: 아래 매크로로 활성화할 앱을 선택하세요
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// App 선택 (하나만 활성화)
#define RUN_LORA_TEST_APP       0   // LoRa 테스트 앱
#define RUN_NETWORK_TEST_APP    1   // 네트워크 테스트 앱

#if RUN_LORA_TEST_APP
    #include "lora_test_app.h"
#elif RUN_NETWORK_TEST_APP
    #include "network_test_app.h"
#endif

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "EoRa-S3 Tally Node");
#if RUN_LORA_TEST_APP
    ESP_LOGI(TAG, "앱: LoRa 테스트");
#elif RUN_NETWORK_TEST_APP
    ESP_LOGI(TAG, "앱: 네트워크 테스트");
#endif
    ESP_LOGI(TAG, "========================================");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if RUN_LORA_TEST_APP

    // LoRa 테스트 앱 초기화 및 시작
    ret = lora_test_app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "앱 초기화 실패");
        return;
    }

    ret = lora_test_app_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "앱 시작 실패");
        return;
    }

#elif RUN_NETWORK_TEST_APP

    // 네트워크 테스트 앱 초기화 및 시작
    ret = network_test_app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "앱 초기화 실패");
        return;
    }

    ret = network_test_app_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "앱 시작 실패");
        return;
    }

    // 주기적으로 상태 출력
    ESP_LOGI(TAG, "5초마다 네트워크 상태 출력...");

#endif

    ESP_LOGI(TAG, "시스템 시작 완료");

    // 메인 루프
    while (1) {
#if RUN_NETWORK_TEST_APP
        // 네트워크 앱: 5초마다 상태 출력
        if (network_test_app_is_running()) {
            network_test_app_print_status();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
#else
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif
    }
}
