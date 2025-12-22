/**
 * @file DisplayManager.h
 * @brief EoRa-S3 U8g2 OLED Display Manager
 *
 * U8g2 라이브러리를 사용한 OLED 디스플레이 관리
 * - I2C SSD1306 128x64
 * - 텍스트 및 그래픽 표시
 * - 부트 화면 및 일반 화면 관리
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Forward declarations for BootScreen functions
#ifdef __cplusplus
extern "C" {
#endif
void BootScreen_showBootScreen(void);
void BootScreen_showBootMessage(const char* message, int progress, int delay_ms);
void BootScreen_bootComplete(bool success, const char* message);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 기본 DisplayManager 함수
esp_err_t DisplayManager_init(void);

// U8g2 인스턴스 접근 함수 (페이지들에서 공통으로 사용)
#include "u8g2.h"
u8g2_t* DisplayManager_getU8g2(void);

// BootScreen 관련 함수 (BootScreen 모듈로 위임)
void DisplayManager_showBootScreen(void);
void DisplayManager_showBootMessage(const char* message, int progress, int delay_ms);
void DisplayManager_bootComplete(bool success, const char* message);

// 일반 화면 관련 함수
void DisplayManager_showNormalScreen(void);
void DisplayManager_stopDisplay(void);

// PageManager 관련 함수
void DisplayManager_initPageManager(void);

// 시스템 정보 구조체
typedef struct {
    uint8_t battery_percent;    // 배터리 (%)
    float temperature_celsius;  // 온도 (°C)
    uint64_t uptime_sec;        // 업타임 (초)
    char wifi_mac[18];          // WiFi MAC 주소
    char device_id[16];         // 장치 ID
    float lora_rssi;            // LoRa RSSI (dBm)
    float lora_snr;             // LoRa SNR (dB)
    bool update_pending;        // 업데이트 필요 플래그
    bool display_changed;       // 디스플레이 업데이트 필요 플래그

    // PGM/PVW 데이터 (RX 모드용)
    uint8_t pgm_list[20];       // PGM 채널 리스트
    uint8_t pgm_count;          // PGM 채널 개수
    uint8_t pvw_list[20];       // PVW 채널 리스트
    uint8_t pvw_count;          // PVW 채널 개수
    bool tally_data_valid;      // Tally 데이터 유효성 플래그

    // 네트워크 정보 (TX 모드용)
    char wifi_ap_ip[16];        // WiFi AP IP 주소
    char wifi_sta_ip[16];       // WiFi STA IP 주소 (연결 안됨면 빈 문자열)
    char eth_ip[16];            // Ethernet IP 주소 (링크 안됨면 빈 문자열)
    bool wifi_sta_connected;    // WiFi STA 연결 상태
    bool eth_link_up;           // Ethernet 링크 상태
} DisplaySystemInfo_t;

// 중앙 관리 시스템 정보 extern 선언
extern DisplaySystemInfo_t s_system_info;

// 시스템 정보 관련 함수
void DisplayManager_startSystemMonitor(void);
void DisplayManager_stopSystemMonitor(void);
DisplaySystemInfo_t DisplayManager_getSystemInfo(void);
void DisplayManager_updateSystemInfo(void);  // 내부용
void DisplayManager_switchToRxPage(void);
void DisplayManager_switchToTxPage(void);
void DisplayManager_setRx1(bool active);
void DisplayManager_setRx2(bool active);

// Tally 데이터 관련 함수 (RX 모드)
void DisplayManager_updateTallyData(const uint8_t* pgm, uint8_t pgm_count,
                                   const uint8_t* pvw, uint8_t pvw_count,
                                   uint8_t total_channels);

// 디스플레이 변경 플래그 관리
void DisplayManager_clearDisplayChangedFlag(void);

/**
 * @brief 시스템 정보 즉시 업데이트 강제
 */
void DisplayManager_forceUpdate(void);

#ifdef DEVICE_MODE_TX
/**
 * @brief 스위처 설정 변경 알림 (ConfigCore에서 호출)
 */
void DisplayManager_onSwitcherConfigChanged(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_MANAGER_H