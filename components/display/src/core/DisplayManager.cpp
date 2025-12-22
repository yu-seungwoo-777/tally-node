/**
 * @file DisplayManager.cpp
 * @brief TALLY-NODE U8g2 OLED Display Manager
 *
 * U8g2 라이브러리를 사용한 OLED 디스플레이 관리
 * TALLY-NODE 프로페셔널 부트 화면 및 일반 화면 관리
 */

#include "core/DisplayManager.h"
#include "u8g2.h"
extern "C" {
#include "u8g2_esp32_hal.h"
}
#include "PinConfig.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <cstring>
#include <cmath>

// SystemMonitor include
extern "C" {
#include "SystemMonitor.h"
}

// InfoManager include
#include "info/info_manager.h"

// LoRaManager forward declarations
extern "C" {
    struct LoRaStatus {
        bool is_initialized;
        int chip_type;
        float frequency;
        float rssi;
        float snr;
        float freq_min;
        float freq_max;
        uint8_t sync_word;
    };

    bool LoRaManager_isInitialized(void);
    struct LoRaStatus LoRaManager_getStatus(void);
}

// Forward declarations for BootScreen functions (C linkage)
extern "C" {
    esp_err_t BootScreen_init(void);
    void BootScreen_showBootScreen(void);
    void BootScreen_showBootMessage(const char* message, int progress, int delay_ms);
    void BootScreen_bootComplete(bool success, const char* message);
}

// Forward declarations for PageManager functions (C linkage)
extern "C" {
    // PageType_t enum
    typedef enum {
        PAGE_TYPE_BOOT = 0,
        PAGE_TYPE_RX,
        PAGE_TYPE_TX,
        PAGE_TYPE_NONE
    } PageType_t;

    // PageManager functions
    esp_err_t PageManager_init(void);
    void PageManager_switchPage(PageType_t page_type);
    void PageManager_handleButton(int button_id);
    void PageManager_update(void);
    void PageManager_updateImmediate(void);
    void PageManager_setRx1(bool active);
    void PageManager_setRx2(bool active);
}


// 부트 스크린 상태
static bool s_boot_complete = false;

// 공통 U8g2 인스턴스
static u8g2_t s_u8g2;
static bool s_u8g2_initialized = false;

// InfoManager Observer 핸들
static info_observer_handle_t s_info_observer_handle = NULL;

// 주의: u8g2_f 버전은 내부적으로 동적 할당을 사용함
// 대신 I2C 동기화를 통해 메모리 충돌 방지

// I2C 동기화를 위한 세마포어
static SemaphoreHandle_t s_display_mutex = NULL;

// 시스템 정보 모니터링
DisplaySystemInfo_t s_system_info = {
    .battery_percent = 75,
    .temperature_celsius = 25.0f,
    .uptime_sec = 0,
    .wifi_mac = "00:00:00:00:00:00",
    .device_id = "????????",      // 기본값
    .lora_rssi = -120.0f,
    .lora_snr = 0.0f,
    .update_pending = false,
    .display_changed = false,

    // PGM/PVW 데이터 초기화
    .pgm_list = {0},
    .pgm_count = 0,
    .pvw_list = {0},
    .pvw_count = 0,
    .tally_data_valid = false,

    // 네트워크 정보 초기화 (TX 모드용)
    .wifi_ap_ip = "192.168.4.1",
    .wifi_sta_ip = "",
    .eth_ip = "",
    .wifi_sta_connected = false,
    .eth_link_up = false
};




// Observer 콜백 함수
static void onSystemInfoChanged(const info_system_info_t* info, void* ctx)
{
    (void)ctx;  // 미사용 파라미터

    if (info == NULL) {
        return;
    }

    // 시스템 정보 업데이트 요청
    s_system_info.battery_percent = info->battery_percent;
    s_system_info.temperature_celsius = info->temperature;
    s_system_info.uptime_sec = info->uptime_sec;
    strncpy(s_system_info.wifi_mac, info->wifi_mac, sizeof(s_system_info.wifi_mac) - 1);
    s_system_info.wifi_mac[sizeof(s_system_info.wifi_mac) - 1] = '\0';

    // Device ID 업데이트
    strncpy(s_system_info.device_id, info->device_id, sizeof(s_system_info.device_id) - 1);
    s_system_info.device_id[sizeof(s_system_info.device_id) - 1] = '\0';

    // LoRa 정보 업데이트 (InfoManager 단위: 0.1dB → DisplayManager 단위: dB)
    s_system_info.lora_rssi = info->lora_rssi / 10.0f;
    s_system_info.lora_snr = info->lora_snr / 10.0f;

    // 즉시 업데이트 요청
    s_system_info.display_changed = true;

    LOG_1(TAG_DISPLAY, "InfoManager에서 시스템 정보 업데이트: Batt=%.1f%%, Temp=%.1f°C, ID=%s",
          s_system_info.battery_percent, s_system_info.temperature_celsius, s_system_info.device_id);
}

// Display가 관리하는 함수들
// 공통 U8g2 인스턴스 getter
u8g2_t* DisplayManager_getU8g2(void)
{
    if (!s_u8g2_initialized) {
        LOG_0(TAG_DISPLAY, "U8g2 not initialized");
        return NULL;
    }
    return &s_u8g2;
}

// 내부 함수: U8g2 초기화
static esp_err_t initU8g2(void)
{
    if (s_u8g2_initialized) {
        return ESP_OK;
    }

    LOG_0(TAG_DISPLAY, "Initializing U8g2 display...");

    // 세마포어 생성
    s_display_mutex = xSemaphoreCreateMutex();
    if (!s_display_mutex) {
        LOG_0(TAG_DISPLAY, "Failed to create display mutex");
        return ESP_ERR_NO_MEM;
    }

    // DisplayBuffer 초기화 (사용하지 않음)

    // U8g2 HAL 설정 (I2C)
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = EORA_S3_I2C_SDA;     // OLED SDA
    u8g2_esp32_hal.bus.i2c.scl = EORA_S3_I2C_SCL;     // OLED SCL
    u8g2_esp32_hal.reset = U8G2_ESP32_HAL_UNDEFINED;  // 리셋 핀 없음
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // U8g2 설정 (SSD1306 128x64 I2C) - 정적 버퍼 사용
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&s_u8g2, U8G2_R0,
                                           u8g2_esp32_i2c_byte_cb,
                                           u8g2_esp32_gpio_and_delay_cb);

    // 정적 버퍼 설정 (동적 할당 제거)
    // u8g2_SetupBuffer가 내부적으로 호출되지만, 우리는 이미 정적 버퍼를 사용
    // u8g2_Setup_ssd1306_i2c_128x64_noname_f가 내부적으로 버퍼를 설정하므로
    // 별도의 정적 버퍼 설정은 필요 없음

    // 디스플레이 초기화
    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);  // 전원 켜기
    u8g2_ClearBuffer(&s_u8g2);

    s_u8g2_initialized = true;

    LOG_0(TAG_DISPLAY, "U8g2 initialized with static buffer");
    return ESP_OK;
}

esp_err_t DisplayManager_init(void)
{
    LOG_0(TAG_DISPLAY, "Initializing DisplayManager...");

    // U8g2 초기화
    esp_err_t ret = initU8g2();
    if (ret != ESP_OK) {
        LOG_0(TAG_DISPLAY, "Failed to initialize U8g2");
        return ret;
    }

    // BootScreen 초기화 (디스플레이 초기화 제외)
    BootScreen_init();

    s_boot_complete = false;

    // InfoManager Observer 등록
    if (info_manager_is_initialized()) {
        esp_err_t err = info_manager_add_observer(onSystemInfoChanged, NULL, &s_info_observer_handle);
        if (err == ESP_OK) {
            LOG_0(TAG_DISPLAY, "InfoManager Observer 등록 성공");
        } else {
            LOG_0(TAG_DISPLAY, "InfoManager Observer 등록 실패: %s", esp_err_to_name(err));
        }
    } else {
        LOG_0(TAG_DISPLAY, "InfoManager 미초기화, Observer 등록 생략");
    }

    LOG_0(TAG_DISPLAY, "DisplayManager initialized successfully");
    return ESP_OK;
}



// BootScreen 관련 함수들 - BootScreen 모듈로 위임
void DisplayManager_showBootScreen(void)
{
    BootScreen_showBootScreen();
}

void DisplayManager_showBootMessage(const char* message, int progress, int delay_ms)
{
    BootScreen_showBootMessage(message, progress, delay_ms);
}

void DisplayManager_bootComplete(bool success, const char* message)
{
    BootScreen_bootComplete(success, message);
    // 성공 시 일반 화면으로 전환
    if (success) {
        DisplayManager_showNormalScreen();
    }
}

// 일반 화면 관리
static TaskHandle_t s_display_task = nullptr;
static volatile bool s_task_running = false;


// 즉각 업데이트를 위한 플래그
static volatile bool s_immediate_update_requested = false;

static void displayTask(void* arg)
{
    (void)arg;

    // 2초 타이머용 변수
    uint64_t last_display_update = 0;
    const uint64_t DISPLAY_UPDATE_INTERVAL = 2000000; // 2초 (마이크로초)

    // 5초 타이머용 변수 (시스템 정보용)
    uint64_t last_system_update = 0;
    const uint64_t SYSTEM_UPDATE_INTERVAL = 5000000; // 5초 (마이크로초)

    while (s_task_running) {
        uint64_t current_time = esp_timer_get_time();

        // 1. 시스템 정보 업데이트 (5초마다)
        if (current_time - last_system_update >= SYSTEM_UPDATE_INTERVAL) {
            DisplayManager_updateSystemInfo();
            last_system_update = current_time;
        }

        // 2. 즉각 업데이트 확인
        if (s_immediate_update_requested) {
            s_immediate_update_requested = false;
            if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                // 즉시 화면 업데이트
                if (s_boot_complete) {
                    PageManager_updateImmediate();
                }
                xSemaphoreGive(s_display_mutex);
            }
        }

        // 3. 2초마다 정기 업데이트
        else if (current_time - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
            if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                last_display_update = current_time;
                // PageManager_update()는 내부적으로 변경 여부를 판단하여 업데이트
                PageManager_update();
                xSemaphoreGive(s_display_mutex);
            }
        }

        // 4. 일반 업데이트 (display_changed 플래그)
        else if (s_system_info.display_changed) {
            if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                s_system_info.display_changed = false;
                PageManager_update();
                xSemaphoreGive(s_display_mutex);
            }
        }

        // 빠른 루프 유지 (10ms 마다 확인)
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(nullptr);
}

void DisplayManager_showNormalScreen(void)
{
    s_boot_complete = true;

    if (!s_task_running) {
        // 일반 디스플레이 태스크 생성 (빠른 루프)
        s_task_running = true;
        xTaskCreatePinnedToCore(displayTask, "display", 8192, nullptr, 3,
                               &s_display_task, 1);  // Core 1, 우선순위 3 (높음)
        LOG_0(TAG_DISPLAY, "Display task started with fast loop");
    }
}

void DisplayManager_stopDisplay(void)
{
    s_task_running = false;
    if (s_display_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_display_task = nullptr;
    }

    // InfoManager Observer 해제
    if (s_info_observer_handle != NULL) {
        if (info_manager_remove_observer(s_info_observer_handle) == ESP_OK) {
            LOG_0(TAG_DISPLAY, "InfoManager Observer 해제 성공");
        }
        s_info_observer_handle = NULL;
    }

    LOG_0(TAG_DISPLAY, "Display task stopped");
}

// PageManager 관련 함수들
void DisplayManager_initPageManager(void)
{
    PageManager_init();
}

void DisplayManager_switchToRxPage(void)
{
    PageManager_switchPage(PAGE_TYPE_RX);
}

void DisplayManager_switchToTxPage(void)
{
    PageManager_switchPage(PAGE_TYPE_TX);
}

void DisplayManager_setRx1(bool active)
{
    PageManager_setRx1(active);
}

void DisplayManager_setRx2(bool active)
{
    PageManager_setRx2(active);
}


// 시스템 정보 업데이트 함수
void DisplayManager_updateSystemInfo(void)
{
    // SystemMonitor에서 실제 데이터 가져오기
    static uint64_t last_update = 0;
    uint64_t current_time = esp_timer_get_time() / 1000000;  // 초 단위

    // 2초마다 업데이트
    if (current_time - last_update >= 2) {
        // 변경 감지를 위한 변수들 (스택 사용 최소화)
        uint8_t old_battery = s_system_info.battery_percent;
        float old_rssi = s_system_info.lora_rssi;
        float old_snr = s_system_info.lora_snr;

        // SystemMonitor에서 실제 시스템 정보 가져오기
        SystemHealth health = SystemMonitor::getHealth();

        s_system_info.battery_percent = health.battery_percent;
        s_system_info.temperature_celsius = health.temperature_celsius;
        s_system_info.uptime_sec = health.uptime_sec;

        // WiFi MAC 주소는 InfoManager에서 관리하므로 여기서 업데이트 안 함

        // LoRa RSSI/SNR 업데이트 (InfoManager에서 가져오기)
        // Device ID 업데이트 (InfoManager에서 가져오기)
        if (info_manager_is_initialized()) {
            info_system_info_t sys_info;
            if (info_manager_get_system_info(&sys_info) == ESP_OK) {
                // InfoManager 단위: 0.1dB → DisplayManager 단위: dB
                // RSSI는 저장할 때 보정했으므로 원복
                if (sys_info.lora_rssi > 5000) {  // 500보다 크면 음수로 간주
                    s_system_info.lora_rssi = ((float)sys_info.lora_rssi / 10.0f) - 1000.0f;
                } else {
                    s_system_info.lora_rssi = (float)sys_info.lora_rssi / 10.0f;
                }
                s_system_info.lora_snr = (float)sys_info.lora_snr / 10.0f;

                // Device ID 업데이트
                strncpy(s_system_info.device_id, sys_info.device_id, sizeof(s_system_info.device_id) - 1);
                s_system_info.device_id[sizeof(s_system_info.device_id) - 1] = '\0';
            } else {
                // 기본값
                s_system_info.lora_rssi = -120.0f;
                s_system_info.lora_snr = 0.0f;
            }
        } else {
            // InfoManager 미초기화 시 기본값 설정
            s_system_info.lora_rssi = -120.0f;
            s_system_info.lora_snr = 0.0f;
        }

        // 디스플레이 관련 값 변경 체크
        if (old_battery != s_system_info.battery_percent ||
            fabs(old_rssi - s_system_info.lora_rssi) > 0.5f ||
            fabs(old_snr - s_system_info.lora_snr) > 0.5f) {
            s_system_info.display_changed = true;
        }

        s_system_info.update_pending = true;
        last_update = current_time;

  #ifdef DEVICE_MODE_TX
        // TX 모드: Batt, Temp만 출력
        LOG_0(TAG_DISPLAY, "System info updated: Batt=%d%%, Temp=%.1f°C",
                 s_system_info.battery_percent,
                 s_system_info.temperature_celsius);
#else
        // RX 모드: Batt, Temp, RSSI, SNR만 출력 (MAC 제거)
        LOG_0(TAG_DISPLAY, "System info updated: Batt=%d%%, Temp=%.1f°C, RSSI=%.1fdBm, SNR=%.1fdB",
                 s_system_info.battery_percent,
                 s_system_info.temperature_celsius,
                 s_system_info.lora_rssi,
                 s_system_info.lora_snr);
#endif
    }
}

// 시스템 모니터링 시작
void DisplayManager_startSystemMonitor(void)
{
    // 이제 displayTask에서 시스템 정보 업데이트를 처리하므로
    // 별도의 타이머가 필요 없음 (과도한 I2C 호출 방지)
    LOG_0(TAG_DISPLAY, "System monitor integrated with display task");
}

// 시스템 모니터링 중지
void DisplayManager_stopSystemMonitor(void)
{
    // 타이머를 사용하지 않으므로 별도의 정리 작업 없음
    LOG_0(TAG_DISPLAY, "System monitor stopped");
}

// 시스템 정보 가져오기
DisplaySystemInfo_t DisplayManager_getSystemInfo(void)
{
    return s_system_info;
}

// Tally 데이터 설정 (CommunicationManager에서 직접 호출)
extern "C" void DisplayManager_setTallyData(const uint8_t* pgm, uint8_t pgm_count,
                                           const uint8_t* pvw, uint8_t pvw_count)
{
    // PGM 데이터 업데이트
    s_system_info.pgm_count = (pgm_count > 20) ? 20 : pgm_count;
    for (uint8_t i = 0; i < s_system_info.pgm_count; i++) {
        s_system_info.pgm_list[i] = pgm[i];
    }

    // PVW 데이터 업데이트
    s_system_info.pvw_count = (pvw_count > 20) ? 20 : pvw_count;
    for (uint8_t i = 0; i < s_system_info.pvw_count; i++) {
        s_system_info.pvw_list[i] = pvw[i];
    }

    s_system_info.tally_data_valid = true;
}

// Tally 데이터 업데이트 (RX 모드) - ISR에서 호출됨
void DisplayManager_updateTallyData(const uint8_t* pgm, uint8_t pgm_count,
                                   const uint8_t* pvw, uint8_t pvw_count,
                                   uint8_t total_channels)
{
    // 즉시 업데이트 플래그 설정 (ISR에서 안전)
    s_immediate_update_requested = true;
}

// 디스플레이 변경 플래그 초기화
void DisplayManager_clearDisplayChangedFlag(void)
{
    s_system_info.display_changed = false;
}


void DisplayManager_forceUpdate(void)
{
    // 즉각 업데이트 요청
    s_immediate_update_requested = true;
}

#ifdef DEVICE_MODE_TX
// 스위처 설정 변경 알림 함수 (C linkage)
extern "C" void DisplayManager_onSwitcherConfigChanged(void)
{
    // 스위처 설정이 변경되었으므로 즉시 업데이트 요청
    LOG_0(TAG_DISPLAY, "스위처 설정 변경 감지 - TX 페이지 업데이트");
    s_immediate_update_requested = true;
}
#endif

