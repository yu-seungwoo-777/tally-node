/**
 * @file TxPage.h
 * @brief TX 모드 페이지 (스위처 연결 상태)
 *
 * 5개 페이지:
 * - Page 1: 스위처 정보 (S1, S2 듀얼 모드 지원)
 * - Page 2: AP (이름, 비밀번호, IP)
 * - Page 3: WIFI (SSID, 비밀번호, IP)
 * - Page 4: ETHERNET (IP, 게이트웨이)
 * - Page 5: 시스템 정보 (3x2 테이블)
 */

#ifndef TX_PAGE_H
#define TX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "DisplayManager.h"

// 스위처 타입
typedef enum {
    SWITCHER_NONE = 0,
    SWITCHER_ATEM,
    SWITCHER_OBS,
    SWITCHER_VMIX
} switcher_type_t;

// 연결 상태
typedef enum {
    TX_STATE_DISCONNECTED = 0,
    TX_STATE_CONNECTING,
    TX_STATE_CONNECTED
} tx_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TxPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool tx_page_init(void);

// ========== 스위처 정보 (Page 1) ==========

/**
 * @brief 듀얼 모드 설정
 * @param dual_mode true = 듀얼 모드
 */
void tx_page_set_dual_mode(bool dual_mode);

/**
 * @brief S1 정보 설정
 * @param type 스위처 타입 ("ATEM", "OBS", "vMix", "NONE")
 * @param ip IP 주소
 * @param port 포트 번호
 * @param connected 연결 상태
 */
void tx_page_set_s1(const char* type, const char* ip, uint16_t port, bool connected);

/**
 * @brief S2 정보 설정
 * @param type 스위처 타입
 * @param ip IP 주소
 * @param port 포트 번호
 * @param connected 연결 상태
 */
void tx_page_set_s2(const char* type, const char* ip, uint16_t port, bool connected);

// ========== AP 정보 (Page 2) ==========

/**
 * @brief AP 이름 설정
 * @param name AP SSID 이름
 */
void tx_page_set_ap_name(const char* name);

/**
 * @brief AP 비밀번호 설정
 * @param password AP 비밀번호
 */
void tx_page_set_ap_password(const char* password);

/**
 * @brief AP IP 설정
 * @param ip IP 주소
 */
void tx_page_set_ap_ip(const char* ip);

// ========== WIFI 정보 (Page 3) ==========

/**
 * @brief WIFI SSID 설정
 * @param ssid WIFI SSID
 */
void tx_page_set_wifi_ssid(const char* ssid);

/**
 * @brief WIFI 비밀번호 설정
 * @param password WIFI 비밀번호
 */
void tx_page_set_wifi_password(const char* password);

/**
 * @brief WIFI IP 설정
 * @param ip IP 주소
 */
void tx_page_set_wifi_ip(const char* ip);

/**
 * @brief WIFI 연결 상태 설정
 * @param connected 연결 상태
 */
void tx_page_set_wifi_connected(bool connected);

// ========== ETHERNET 정보 (Page 4) ==========

/**
 * @brief Ethernet IP 설정
 * @param ip IP 주소
 */
void tx_page_set_eth_ip(const char* ip);

/**
 * @brief Ethernet 게이트웨이 설정
 * @param gateway 게이트웨이 주소
 */
void tx_page_set_eth_gateway(const char* gateway);

/**
 * @brief Ethernet 연결 상태 설정
 * @param connected 연결 상태
 */
void tx_page_set_eth_connected(bool connected);

// ========== 시스템 정보 (Page 5) ==========

/**
 * @brief 배터리 퍼센트 설정
 * @param percent 배터리 퍼센트 (0-100)
 */
void tx_page_set_battery(uint8_t percent);

/**
 * @brief 주파수 설정
 * @param freq_mhz 주파수 (MHz)
 */
void tx_page_set_frequency(float freq_mhz);

/**
 * @brief 동기 워드 설정
 * @param sync_word 동기 워드
 */
void tx_page_set_sync_word(uint8_t sync_word);

/**
 * @brief 전압 설정
 * @param voltage 전압 (V)
 */
void tx_page_set_voltage(float voltage);

/**
 * @brief 온도 설정
 * @param temp 온도 (°C)
 */
void tx_page_set_temperature(float temp);

/**
 * @brief Device ID 설정
 * @param device_id Device ID (최대 16자)
 */
void tx_page_set_device_id(const char* device_id);

/**
 * @brief Uptime 설정
 * @param uptime_sec 가동 시간 (초)
 */
void tx_page_set_uptime(uint64_t uptime_sec);

/**
 * @brief LoRa RSSI 설정
 * @param rssi RSSI 값 (dBm)
 */
void tx_page_set_rssi(int16_t rssi);

/**
 * @brief LoRa SNR 설정
 * @param snr SNR 값 (dB)
 */
void tx_page_set_snr(float snr);

// ========== 페이지 제어 ==========

/**
 * @brief 페이지 전환
 * @param page 페이지 번호 (1: Switcher, 2: AP, 3: WIFI, 4: ETHERNET, 5: System)
 */
void tx_page_switch_page(uint8_t page);

/**
 * @brief 현재 페이지 가져오기
 * @return 페이지 번호 (1: Switcher, 2: AP, 3: WIFI, 4: ETHERNET, 5: System)
 */
uint8_t tx_page_get_current_page(void);

#ifdef __cplusplus
}
#endif

#endif // TX_PAGE_H
