/**
 * @file main.cpp
 * @brief EoRa-S3 Application Layer
 *
 * 아키텍처:
 * Application (main.cpp)
 *    ↓
 * Manager Layer (NetworkManager, TallyDispatcher, DisplayManager)
 *    ↓
 * Core API Layer (WiFiCore, EthernetCore, LoRaCore, OLEDCore, ConfigCore, CLICore)
 *    ↓
 * Hardware / External Libs (ESP-IDF, RadioLib, Switcher)
 *
 * 디바이스 모드:
 * - TX (Transmitter): Switcher → LoRa 송신 (네트워크 필요)
 * - RX (Receiver): LoRa 수신 → Tally 표시 (네트워크 불필요)
 */

#include <stdio.h>
#include "esp_log.h"
#include "log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

// Config module
#include "ConfigCore.h"

// Display module
#include "DisplayManager.h"

// System Monitor
#include "SystemMonitor.h"

// Communication module (공통)
#include "LoRaCore.h"
#include "LoRaManager.h"
#include "TallyDispatcher.h"

// TX 전용 모듈
#ifdef DEVICE_MODE_TX
#include "log.h"
#include "NetworkManager.h"
#include "SwitcherManager.h"
#include "WebServerCore.h"
extern "C" {
#include "switcher.h"
}
#endif

// Interface module (Core)
#include "CLICore.h"

// Button module (RX mode)
#ifdef DEVICE_MODE_RX
#include "button_poll.h"
#include "button_actions.h"
#endif

// ConfigCore에 버튼 기능 포함

// LED module (RX 전용)
#ifdef DEVICE_MODE_RX
extern "C" {
#include "WS2812Core.h"
}
#endif

// Log tags
#include "log_tags.h"

// InfoManager
#include "info/info_manager.h"

static const char* TAG = TAG_MAIN;

// 태스크 핸들
static TaskHandle_t s_communication_task = nullptr;


/**
 * @brief 통신 업데이트 태스크 (Core 0, Hot Path)
 *
 * TX: SwitcherManager loop + TallyDispatcher loop
 * RX: TallyDispatcher loop만
 */
static void communicationTask(void* arg)
{
    (void)arg;

    while (true) {
#ifdef DEVICE_MODE_TX
        // TX: Hot Path - SwitcherManager loop (모든 스위처 업데이트)
        SwitcherManager::loop();

        // Tally 변경 처리 및 LoRa 전송
        TallyDispatcher::processTallyChanges();
#endif

        // 다른 태스크에 양보 (5ms delay)
        // Tally 응답성 향상 (200Hz)
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**
 * @brief 애플리케이션 메인
 */
extern "C" void app_main(void)
{
    // ESP-IDF 기본 로그 비활성화
    esp_log_level_set("*", ESP_LOG_NONE);

    // 새로운 로그 시스템 초기화
    log_init(LOG_LEVEL_0);

    // ========================================
    // 부팅 시작
    // ========================================
    LOG_0(TAG, "");
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "EoRa-S3 Tally System");
    LOG_0(TAG, "ESP-IDF %s", esp_get_idf_version());
#ifdef DEVICE_MODE_TX
    LOG_0(TAG, "Mode: TX (Switcher → LoRa)");
#elif defined(DEVICE_MODE_RX)
    LOG_0(TAG, "Mode: RX (LoRa → Tally)");
#else
    LOG_0(TAG, "Mode: UNKNOWN");
#endif
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "");

    // ========================================
    // 1단계: 기본 시스템 초기화
    // ========================================
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "[1/4] 기본 시스템 초기화");
    LOG_0(TAG, "=================================");

    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // NVS 초기화
    DisplayManager_showBootMessage("Initializing...", 5, 300);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // InfoManager 초기화 (가장 먼저 초기화)
    DisplayManager_showBootMessage("Info Init...", 8, 300);
    ret = info_manager_init();
    if (ret != ESP_OK) {
        LOG_0(TAG, "✗ InfoManager 초기화 실패: %s", esp_err_to_name(ret));
        return;
    } else {
        // Observer에게 초기화된 시스템 정보 알림
        info_manager_update_system_info();
        info_manager_notify_observers();

        // 장치 ID 로깅
        char device_id[INFO_DEVICE_ID_MAX_LEN];
        if (info_manager_get_device_id(device_id, sizeof(device_id)) == ESP_OK) {
            LOG_0(TAG, "✓ InfoManager 초기화 완료 (Device ID: %s)", device_id);
        }
    }

    // DisplayManager 초기화 (먼저 한 번만 초기화)
    DisplayManager_showBootMessage("Display Init...", 10, 300);
    if (DisplayManager_init() != ESP_OK) {
        LOG_0(TAG, "✗ DisplayManager 초기화 실패");
        return;
    } else {
        LOG_0(TAG, "✓ DisplayManager 초기화 완료");
        // 부트 스크린 시작
        DisplayManager_showBootScreen();
    }

    // ConfigCore 초기화 (NVS 설정 로드)
    DisplayManager_showBootMessage("Loading Config...", 15, 400);
    if (ConfigCore::init() != ESP_OK) {
        LOG_0(TAG, "✗ ConfigCore 초기화 실패");
        return;
    }

      // SystemMonitor 초기화
    if (SystemMonitor::init() != ESP_OK) {
        LOG_0(TAG, "✗ SystemMonitor 초기화 실패");
        return;
    }

    // DisplayManager 시스템 모니터링 시작
    DisplayManager_startSystemMonitor();

    LOG_0(TAG, "✓ 기본 시스템 준비 완료");
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "");

#ifdef DEVICE_MODE_TX
    // ========================================
    // 2단계: 네트워크 및 웹서버 초기화 (TX 전용)
    // ========================================
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "[2/4] 네트워크 및 웹서버 초기화");
    LOG_0(TAG, "=================================");

    if (NetworkManager::init() != ESP_OK) {
        LOG_0(TAG, "✗ NetworkManager 초기화 실패");
        return;
    }

    // WiFi STA 연결 대기 (최대 10초)
    int wait_count = 0;
    while (wait_count < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        NetworkStatus net_status = NetworkManager::getStatus();
        if (net_status.wifi_sta.connected) {
            break;
        }
        wait_count++;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));  // 네트워크 안정화

    // 웹서버 시작
    if (WebServerCore::init() != ESP_OK) {
        LOG_0(TAG, "✗ 웹서버 시작 실패");
    }

    // 네트워크 상태 통합 출력
    NetworkStatus net_status = NetworkManager::getStatus();
    LOG_0(TAG, "네트워크 상태:");
    LOG_0(TAG, "WiFi AP:  %s", net_status.wifi_ap.ip);
    if (net_status.wifi_sta.connected) {
        LOG_0(TAG, "WiFi STA: %s", net_status.wifi_sta.ip);
    } else {
        LOG_0(TAG, "WiFi STA: 연결 안됨");
    }
    if (net_status.eth_detail.link_up) {
        LOG_0(TAG, "Ethernet: %s", net_status.ethernet.ip);
    } else {
        LOG_0(TAG, "Ethernet: 링크 없음");
    }
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "");

    // ========================================
    // 3단계: 스위처 초기화 (TX 전용)
    // ========================================
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "[3/4] 스위처 초기화");
    LOG_0(TAG, "=================================");

    if (SwitcherManager::init() != ESP_OK) {
        LOG_0(TAG, "✗ SwitcherManager 초기화 실패");
        return;
    }
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "");
#endif

    // ========================================
    // CLI 초기화
    DisplayManager_showBootMessage("Init CLI...", 25, 300);
    if (CLICore::init() != ESP_OK) {
        LOG_0(TAG, "✗ CLI 초기화 실패");
    }

    // LoRa 무선 모듈 초기화
    DisplayManager_showBootMessage("Init LoRa...", 40, 800);
    if (LoRaManager::init() != ESP_OK) {
        LOG_0(TAG, "✗ LoRa 초기화 실패");
        return;
    }

    // 통신 매니저 초기화
    DisplayManager_showBootMessage("Init Network...", 55, 500);
    if (TallyDispatcher::init() != ESP_OK) {
        LOG_0(TAG, "✗ TallyDispatcher 초기화 실패");
        return;
    }

// 버튼 초기화 (RX/TX 모드 모두)
    if (button_poll_init() != ESP_OK) {
        LOG_0(TAG, "✗ 버튼 폴링 초기화 실패");
    }

    if (button_poll_start() != ESP_OK) {
        LOG_0(TAG, "✗ 버튼 폴링 시작 실패");
    }

    LOG_0(TAG, "✓ 버튼 시스템 초기화 완료");

#ifdef DEVICE_MODE_RX
    // RX 모드: WS2812 LED 초기화 전 GPIO 강제 리셋 (TALLY_NODE 방식 적용)
    gpio_reset_pin((gpio_num_t)EORA_S3_LED_WS2812);
    gpio_set_direction((gpio_num_t)EORA_S3_LED_WS2812, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)EORA_S3_LED_WS2812, 0);
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 대기

    // WS2812 LED 초기화
    if (WS2812Core_initDefault() != ESP_OK) {
        LOG_0(TAG, "✗ WS2812 초기화 실패");
    }

    // 여러 번 LED OFF로 설정 (TALLY_NODE 방식 적용)
    for (int i = 0; i < 3; i++) {
        WS2812Core_setState(WS2812_OFF);
        vTaskDelay(pdMS_TO_TICKS(5));  // 5ms 대기
    }
    LOG_0(TAG, "LED 초기 상태: OFF (3번 반복)");

    // ConfigCore에서 LED 밝기 설정 적용
    const auto& config = ConfigCore::getAll();
    WS2812Core_setBrightness(config.led_brightness);
    LOG_0(TAG, "LED 밝기 설정: %d/255", config.led_brightness);
#endif

    LOG_0(TAG, "✓ CLI, LoRa, Communication 준비 완료");
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "");

    
    // 태스크 생성
    // Communication: Core 1 (LWIP는 Core 0에서 실행되므로 분리)
    xTaskCreatePinnedToCore(communicationTask, "communication", 8192, nullptr, 8,
                           &s_communication_task, 1);

    // ========================================
    // 부팅 완료
    // ========================================
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "시스템 시작 완료");
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "");

    // 최종 시스템 준비
    DisplayManager_showBootMessage("Starting...", 75, 800);
    DisplayManager_bootComplete(true, nullptr);  // nullptr로 하여 "System Ready" 기본 메시지 사용

// PageManager 초기화
DisplayManager_initPageManager();

#ifdef DEVICE_MODE_RX
    // RX 모드: 부팅 후 RX 페이지로 전환
    vTaskDelay(pdMS_TO_TICKS(2000));  // 부팅 완료 메시지가 보이도록 잠시 대기

    // RX 페이지로 전환
    DisplayManager_switchToRxPage();

    // 초기 상태 설정 (RX1만 활성)
    DisplayManager_setRx1(true);
    DisplayManager_setRx2(false);

    LOG_0(TAG, "✓ RX 페이지로 전환 완료");
#else
    // TX 모드: 부팅 후 TX 페이지로 전환
    vTaskDelay(pdMS_TO_TICKS(2000));

    // TX 페이지로 전환
    DisplayManager_switchToTxPage();

    LOG_0(TAG, "✓ TX 페이지로 전환 완료");
#endif

    // 메인 태스크는 무한 대기 (디스플레이 태스크가 처리)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초마다 체크

        // TX 모드: 네트워크 정보 업데이트
        #ifdef DEVICE_MODE_TX
        static uint64_t last_network_update = 0;
        uint64_t current_time = esp_timer_get_time() / 1000000;
        if (current_time - last_network_update >= 5) {  // 5초마다 업데이트
            NetworkStatus net_status = NetworkManager::getStatus();

            // 외부 선언된 DisplayManager의 전역 변수에 직접 접근
            extern DisplaySystemInfo_t s_system_info;

            // WiFi AP (항상 활성화)
            strncpy(s_system_info.wifi_ap_ip, net_status.wifi_ap.ip, sizeof(s_system_info.wifi_ap_ip) - 1);
            s_system_info.wifi_ap_ip[sizeof(s_system_info.wifi_ap_ip) - 1] = '\0';

            // WiFi STA
            s_system_info.wifi_sta_connected = net_status.wifi_sta.connected;
            if (net_status.wifi_sta.connected) {
                strncpy(s_system_info.wifi_sta_ip, net_status.wifi_sta.ip, sizeof(s_system_info.wifi_sta_ip) - 1);
                s_system_info.wifi_sta_ip[sizeof(s_system_info.wifi_sta_ip) - 1] = '\0';
            } else {
                s_system_info.wifi_sta_ip[0] = '\0';
            }

            // Ethernet
            s_system_info.eth_link_up = net_status.eth_detail.link_up;
            if (net_status.eth_detail.link_up) {
                strncpy(s_system_info.eth_ip, net_status.eth_detail.ip, sizeof(s_system_info.eth_ip) - 1);
                s_system_info.eth_ip[sizeof(s_system_info.eth_ip) - 1] = '\0';
            } else {
                s_system_info.eth_ip[0] = '\0';
            }

            // 디스플레이 업데이트 플래그 설정
            s_system_info.display_changed = true;

            last_network_update = current_time;
        }
        #endif
    }
}
