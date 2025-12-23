/**
 * @file main.cpp
 * @brief EoRa-S3 메인
 *
 * 01_app 계층 - 애플리케이션 진입점
 * - NVS 초기화
 * - 앱 실행
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// App
#include "lora_test_app.h"

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "EoRa-S3 Tally Node");
    ESP_LOGI(TAG, "========================================");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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

    ESP_LOGI(TAG, "시스템 시작 완료");

    // 메인 루프 (유휴 상태)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
