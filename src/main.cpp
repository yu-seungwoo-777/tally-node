/**
 * @file main.cpp
 * @brief EoRa-S3 메인
 *
 * 01_app 계층 - 애플리케이션 진입점
 * - NVS 초기화
 * - 앱 실행
 */

#include <stdio.h>
#include "t_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 앱 모드 선택 (0 또는 1로 설정, 하나만 1로 활성화)
// ============================================================================

#define RUN_ETH_TEST        0   // 이더넷 테스트 모드 (W5500만)
#define RUN_TALLY_TX_APP    1   // Tally TX 앱 (스위처 연결 + LoRa 송신)
#define RUN_TALLY_RX_APP    0   // Tally RX 앱 (LoRa 수신)

#if RUN_ETH_TEST
    #include "EthernetHal.h"
    #include "esp_netif.h"
#elif RUN_TALLY_TX_APP
    #include "tally_tx_app.h"
#elif RUN_TALLY_RX_APP
    #include "tally_rx_app.h"
#endif

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    T_LOGI(TAG, "========================================");
    T_LOGI(TAG, "EoRa-S3 Tally Node");
#if RUN_ETH_TEST
    T_LOGI(TAG, "앱: 이더넷 테스트 (W5500 only)");
#elif RUN_TALLY_TX_APP
    T_LOGI(TAG, "앱: Tally TX (스위처 연결 + LoRa 송신)");
#elif RUN_TALLY_RX_APP
    T_LOGI(TAG, "앱: Tally RX (LoRa 수신)");
#endif
    T_LOGI(TAG, "========================================");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if RUN_ETH_TEST

    // ============================================================================
    // 이더넷 테스트 모드
    // ============================================================================

    T_LOGI(TAG, "");
    T_LOGI(TAG, "===== 이더넷 테스트 모드 =====");
    T_LOGI(TAG, "");

    // netif 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "esp_netif_init 실패: %s", esp_err_to_name(ret));
        return;
    }

    // Ethernet HAL 초기화
    ret = ethernet_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Ethernet HAL 초기화 실패: %s", esp_err_to_name(ret));
        return;
    }
    T_LOGI(TAG, "Ethernet HAL 초기화 완료");

    // Ethernet 시작
    ret = ethernet_hal_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Ethernet 시작 실패: %s", esp_err_to_name(ret));
        return;
    }
    T_LOGI(TAG, "Ethernet 시작 완료");

    // DHCP 활성화
    ret = ethernet_hal_enable_dhcp();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "DHCP 활성화 실패: %s", esp_err_to_name(ret));
    } else {
        T_LOGI(TAG, "DHCP 활성화 완료");
    }

    T_LOGI(TAG, "");
    T_LOGI(TAG, "이더넷 링크업 및 IP 획득 대기 중...");
    T_LOGI(TAG, "");

#elif RUN_TALLY_TX_APP

    // Tally TX 앱 초기화 및 시작 (기본 설정 사용)
    if (!tally_tx_app_init(nullptr)) {
        T_LOGE(TAG, "앱 초기화 실패");
        return;
    }

    tally_tx_app_start();

    T_LOGI(TAG, "스위처 연결 대기 중...");

#elif RUN_TALLY_RX_APP

    // Tally RX 앱 초기화 및 시작 (기본 설정 사용)
    if (!tally_rx_app_init(nullptr)) {
        T_LOGE(TAG, "앱 초기화 실패");
        return;
    }

    tally_rx_app_start();

    T_LOGI(TAG, "LoRa 수신 대기 중...");

#endif

    T_LOGI(TAG, "시스템 시작 완료");

    // 메인 루프
    while (1) {
#if RUN_ETH_TEST
        // 이더넷 상태 주기적 출력 (5초 간격)
        static int cnt = 0;
        if (++cnt >= 5) {
            cnt = 0;
            ethernet_hal_status_t status;
            if (ethernet_hal_get_status(&status) == ESP_OK) {
                T_LOGI(TAG, "");
                T_LOGI(TAG, "===== 이더넷 상태 =====");
                T_LOGI(TAG, "초기화: %s", status.initialized ? "예" : "아니오");
                T_LOGI(TAG, "링크:    %s", status.link_up ? "UP" : "DOWN");
                T_LOGI(TAG, "IP:      %s", status.got_ip ? status.ip : "없음");
                T_LOGI(TAG, "MAC:     %s", status.mac);
                T_LOGI(TAG, "======================");
                T_LOGI(TAG, "");
            }
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
