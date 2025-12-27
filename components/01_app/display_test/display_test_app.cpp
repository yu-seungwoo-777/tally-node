/**
 * @file display_test_app.cpp
 * @brief 디스플레이 테스트 앱 구현
 *
 * BootPage를 표시하고 진행률을 업데이트하는 테스트 앱
 * 부팅 완료 후 빌드 환경에 따라 TX/RX 페이지 자동 전환
 * 버튼으로 페이지 내 전환 테스트 지원
 */

#include "display_test_app.h"
#include "DisplayManager.h"
#include "TxPage.h"
#include "RxPage.h"
#include "button_poll.h"
#include "button_handler.h"
#include "t_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG = "DisplayTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t display_test_app_init(void)
{
    T_LOGI(TAG, "========================================");
    T_LOGI(TAG, "디스플레이 테스트 앱 초기화");
    T_LOGI(TAG, "========================================");

    // DisplayManager 초기화 (내부에서 BootPage 자동 초기화)
    T_LOGI(TAG, "DisplayManager 초기화 중...");
    if (!display_manager_init()) {
        T_LOGE(TAG, "DisplayManager 초기화 실패");
        return ESP_FAIL;
    }
    T_LOGI(TAG, "DisplayManager 초기화 완료");

    // TxPage, RxPage 등록
    T_LOGI(TAG, "페이지 등록 중...");
    if (!tx_page_init()) {
        T_LOGE(TAG, "TxPage 초기화 실패");
        return ESP_FAIL;
    }
    if (!rx_page_init()) {
        T_LOGE(TAG, "RxPage 초기화 실패");
        return ESP_FAIL;
    }
    T_LOGI(TAG, "페이지 등록 완료");

    // 버튼 폴링 초기화
    T_LOGI(TAG, "버튼 폴링 초기화 중...");
    esp_err_t ret = button_poll_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "버튼 폴링 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 버튼 핸들러 초기화
    T_LOGI(TAG, "버튼 핸들러 초기화 중...");
    ret = button_handler_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "버튼 핸들러 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 초기화 완료");
    return ESP_OK;
}

esp_err_t display_test_app_start(void)
{
    if (s_running) {
        T_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    T_LOGI(TAG, "디스플레이 테스트 앱 시작 중...");

    // DisplayManager 시작
    display_manager_start();

    // BootPage로 전환
    display_manager_set_page(PAGE_BOOT);

    // 버튼 폴링 시작
    esp_err_t ret = button_poll_start();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "버튼 폴링 시작 실패: %s", esp_err_to_name(ret));
    }

    // 버튼 핸들러 시작
    ret = button_handler_start();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "버튼 핸들러 시작 실패: %s", esp_err_to_name(ret));
    }

    s_running = true;
    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 시작 완료");

    // 부팅 시나리오 실행 (메시지 업데이트)
    T_LOGI(TAG, "");
    T_LOGI(TAG, "===== 부팅 시나리오 시작 =====");

    const char* boot_messages[] = {
        "LoRa init...",
        "Network init...",
        "Loading config...",
        "Starting services...",
        "System ready"
    };

    for (int i = 0; i < 5; i++) {
        int progress = (i + 1) * 20;
        display_manager_boot_set_message(boot_messages[i]);
        display_manager_boot_set_progress(progress);
        T_LOGI(TAG, "[%d%%] %s", progress, boot_messages[i]);
        display_manager_force_refresh();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    T_LOGI(TAG, "===== 부팅 시나리오 완료 =====");
    T_LOGI(TAG, "");

    // 테스트 데이터 설정 (TX/RX 공통)
#ifdef DEVICE_MODE_TX
    T_LOGI(TAG, "TX 모드 - 5개 페이지");
    T_LOGI(TAG, "  - 짧게 누르기: Switcher -> AP -> WIFI -> ETHERNET -> System -> Switcher...");

    tx_page_set_dual_mode(true);
    tx_page_set_s1("ATEM", "192.168.1.100", 9910, true);
    tx_page_set_s2("OBS", "192.168.1.101", 4455, true);

    // AP 정보 (Page 2)
    tx_page_set_ap_name("TallyNode-AP");
    tx_page_set_ap_ip("192.168.4.1");

    // WIFI 정보 (Page 3)
    tx_page_set_wifi_ssid("MyWiFi");
    tx_page_set_wifi_ip("192.168.1.50");
    tx_page_set_wifi_connected(true);

    // ETHERNET 정보 (Page 4)
    tx_page_set_eth_ip("10.0.0.50");
    tx_page_set_eth_dhcp_mode(true);
    tx_page_set_eth_connected(true);

    // 시스템 정보 (Page 5)
    tx_page_set_battery(75);
    tx_page_set_frequency(868.0f);
    tx_page_set_sync_word(0x12);
    tx_page_set_voltage(3.7f);
    tx_page_set_temperature(25.0f);
    tx_page_set_device_id("AABBCCDD");
    tx_page_set_uptime(7200);
#elif defined(DEVICE_MODE_RX)
    T_LOGI(TAG, "RX 모드 - Tally/System 페이지");
    T_LOGI(TAG, "  - 짧게 누르기: Tally 페이지 <-> 시스템 페이지");

    rx_page_set_cam_id(1);
    rx_page_set_battery(85);
    rx_page_set_rssi(-65);
    rx_page_set_snr(9.5f);
    rx_page_set_frequency(868.0f);
    rx_page_set_sync_word(0x12);
    rx_page_set_voltage(3.85f);
    rx_page_set_temperature(28.5f);
    rx_page_set_device_id("AABBCCDD");
    rx_page_set_uptime(3600);

    uint8_t pgm_ch[] = {1, 3};
    rx_page_set_pgm_channels(pgm_ch, 2);
    uint8_t pvw_ch[] = {2};
    rx_page_set_pvw_channels(pvw_ch, 1);
#endif
    T_LOGI(TAG, "");

    // DisplayManager에서 빌드 환경에 따라 자동 전환
    display_manager_boot_complete();

    return ESP_OK;
}

void display_test_app_stop(void)
{
    if (!s_running) {
        return;
    }

    T_LOGI(TAG, "디스플레이 테스트 앱 정지 중...");
    s_running = false;
    T_LOGI(TAG, "✓ 디스플레이 테스트 앱 정지 완료");
}

bool display_test_app_is_running(void)
{
    return s_running;
}

} // extern "C"
