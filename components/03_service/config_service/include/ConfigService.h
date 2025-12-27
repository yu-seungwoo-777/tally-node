/**
 * @file ConfigService.h
 * @brief NVS 설정 관리 서비스 (C++)
 *
 * 역할: NVS(Non-Volatile Storage)를 사용한 설정 관리
 * - WiFi AP 설정
 * - WiFi STA 설정
 * - Ethernet 설정
 * - Device 설정 (brightness, camera_id, RF)
 * - System 상태 (device_id, battery, uptime, stopped)
 * - 설정 로드/저장
 * - 기본값 관리
 */

#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// C 구조체 (외부 인터페이스)
// ============================================================================

// WiFi AP 설정
typedef struct {
    char ssid[33];
    char password[65];
    uint8_t channel;
    bool enabled;
} config_wifi_ap_t;

// WiFi STA 설정
typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} config_wifi_sta_t;

// Ethernet 설정
typedef struct {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
    bool enabled;
} config_ethernet_t;

// Switcher 설정
typedef struct {
    uint8_t type;          // 0=ATEM, 1=OBS, 2=vMix
    char ip[16];          // IP 주소
    uint16_t port;        // 포트 (0=기본값)
    char password[64];    // 비밀번호
    uint8_t interface;    // 1=WiFi, 2=Ethernet
    uint8_t camera_limit; // 카메라 제한 (0=무제한)
} config_switcher_t;

// RF 설정 (LoRa)
typedef struct {
    float frequency;     // MHz (예: 868.0f)
    uint8_t sync_word;   // Sync Word (예: 0x12)
    uint8_t sf;          // Spreading Factor (7-12)
    uint8_t cr;          // Coding Rate (5-8)
    float bw;           // Bandwidth kHz (125/250/500)
    int8_t tx_power;    // TX Power dBm (-22 ~ +22)
} config_rf_t;

// LED 색상 설정 (RGB)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} config_led_color_t;

// LED 색상 설정 (NVS 저장)
typedef struct {
    config_led_color_t program;      // PROGRAM 상태 색상 (빨강)
    config_led_color_t preview;      // PREVIEW 상태 색상 (초록)
    config_led_color_t off;          // OFF 상태 색상 (검정, 변경 가능)
    config_led_color_t battery_low;  // BATTERY_LOW 상태 색상 (노랑)
} config_led_colors_t;

// Device 설정 (NVS 저장)
typedef struct {
    uint8_t brightness;      // 0-255
    uint8_t camera_id;       // 카메라 ID
    config_rf_t rf;          // RF 설정
} config_device_t;

// System 상태 (RAM, 저장 안 함)
typedef struct {
    char device_id[5];       // 디바이스 ID (4자리 hex 문자열, 읽기 전용)
    uint8_t battery;         // 배터리 %
    uint32_t uptime;         // 업타임 (초)
    bool stopped;            // 기능 정지 상태
    int16_t rssi;            // LoRa RSSI (dBm)
    float snr;               // LoRa SNR (dB)
} config_system_t;

// 전체 설정
typedef struct {
    config_wifi_ap_t wifi_ap;
    config_wifi_sta_t wifi_sta;
    config_ethernet_t ethernet;
    config_device_t device;       // Device 설정 (NVS 저장)
    config_switcher_t primary;    // Primary Switcher (NVS 저장)
    config_switcher_t secondary;   // Secondary Switcher (NVS 저장)
    bool dual_enabled;             // Dual Mode 활성화
    uint8_t secondary_offset;      // Secondary 오프셋 (0~19)
} config_all_t;

// ============================================================================
// C 인터페이스
// ============================================================================

/**
 * @brief Config Service 초기화
 */
esp_err_t config_service_init(void);

/**
 * @brief 전체 설정 로드
 */
esp_err_t config_service_load_all(config_all_t* config);

/**
 * @brief 전체 설정 저장
 */
esp_err_t config_service_save_all(const config_all_t* config);

/**
 * @brief WiFi AP 설정 로드
 */
esp_err_t config_service_get_wifi_ap(config_wifi_ap_t* config);

/**
 * @brief WiFi AP 설정 저장
 */
esp_err_t config_service_set_wifi_ap(const config_wifi_ap_t* config);

/**
 * @brief WiFi STA 설정 로드
 */
esp_err_t config_service_get_wifi_sta(config_wifi_sta_t* config);

/**
 * @brief WiFi STA 설정 저장
 */
esp_err_t config_service_set_wifi_sta(const config_wifi_sta_t* config);

/**
 * @brief Ethernet 설정 로드
 */
esp_err_t config_service_get_ethernet(config_ethernet_t* config);

/**
 * @brief Ethernet 설정 저장
 */
esp_err_t config_service_set_ethernet(const config_ethernet_t* config);

// ============================================================================
// Switcher 설정 API
// ============================================================================

/**
 * @brief Primary Switcher 로드
 */
esp_err_t config_service_get_primary(config_switcher_t* config);

/**
 * @brief Primary Switcher 저장
 */
esp_err_t config_service_set_primary(const config_switcher_t* config);

/**
 * @brief Secondary Switcher 로드
 */
esp_err_t config_service_get_secondary(config_switcher_t* config);

/**
 * @brief Secondary Switcher 저장
 */
esp_err_t config_service_set_secondary(const config_switcher_t* config);

/**
 * @brief Dual Mode 활성화 로드
 */
bool config_service_get_dual_enabled(void);

/**
 * @brief Dual Mode 활성화 저장
 */
esp_err_t config_service_set_dual_enabled(bool enabled);

/**
 * @brief Secondary 오프셋 로드
 */
uint8_t config_service_get_secondary_offset(void);

/**
 * @brief Secondary 오프셋 저장
 */
esp_err_t config_service_set_secondary_offset(uint8_t offset);

// ============================================================================
// Device 설정 API
// ============================================================================

/**
 * @brief Device 설정 로드
 */
esp_err_t config_service_get_device(config_device_t* config);

/**
 * @brief Device 설정 저장
 */
esp_err_t config_service_set_device(const config_device_t* config);

/**
 * @brief 밝기 설정
 */
esp_err_t config_service_set_brightness(uint8_t brightness);

/**
 * @brief 카메라 ID 설정
 */
esp_err_t config_service_set_camera_id(uint8_t camera_id);

/**
 * @brief 카메라 ID 가져오기
 */
uint8_t config_service_get_camera_id(void);

/**
 * @brief 최대 카메라 번호 가져오기
 * @return 최대 카메라 번호 (기본값: 20)
 */
uint8_t config_service_get_max_camera_num(void);

/**
 * @brief RF 설정 (주파수 + Sync Word)
 */
esp_err_t config_service_set_rf(float frequency, uint8_t sync_word);

// ============================================================================
// System 상태 API
// ============================================================================

/**
 * @brief Device ID 가져오기 (4자리 hex 문자열)
 */
const char* config_service_get_device_id(void);

/**
 * @brief System 상태 가져오기
 */
void config_service_get_system(config_system_t* status);

/**
 * @brief Battery 설정
 */
void config_service_set_battery(uint8_t battery);

/**
 * @brief Battery ADC로 읽기 (백분율 반환)
 */
uint8_t config_service_update_battery(void);

/**
 * @brief 전압 가져오기 (V)
 * @return 전압 (V)
 */
float config_service_get_voltage(void);

/**
 * @brief 온도 가져오기 (°C)
 * @return 온도 (°C)
 */
float config_service_get_temperature(void);

/**
 * @brief RSSI 가져오기 (dBm)
 * @return RSSI (dBm)
 */
int16_t config_service_get_rssi(void);

/**
 * @brief SNR 가져오기 (dB)
 * @return SNR (dB)
 */
float config_service_get_snr(void);

/**
 * @brief Stopped 상태 설정
 */
void config_service_set_stopped(bool stopped);

/**
 * @brief Uptime 증가 (1초 타이머 호출용)
 */
void config_service_inc_uptime(void);

// ============================================================================
// LED 색상 설정 API
// ============================================================================

/**
 * @brief LED 색상 설정 로드
 */
esp_err_t config_service_get_led_colors(config_led_colors_t* config);

/**
 * @brief LED 색상 설정 저장
 */
esp_err_t config_service_set_led_colors(const config_led_colors_t* config);

/**
 * @brief PROGRAM 색상 가져오기
 */
void config_service_get_led_program_color(uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * @brief PREVIEW 색상 가져오기
 */
void config_service_get_led_preview_color(uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * @brief OFF 색상 가져오기
 */
void config_service_get_led_off_color(uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * @brief BATTERY_LOW 색상 가져오기
 */
void config_service_get_led_battery_low_color(uint8_t* r, uint8_t* g, uint8_t* b);

// ============================================================================
// 기존 API
// ============================================================================

/**
 * @brief 기본값 로드
 */
esp_err_t config_service_load_defaults(config_all_t* config);

/**
 * @brief 공장 초기화
 */
esp_err_t config_service_factory_reset(void);

/**
 * @brief 초기화 여부
 */
bool config_service_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_SERVICE_H
