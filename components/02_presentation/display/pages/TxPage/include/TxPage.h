/**
 * @file TxPage.h
 * @brief TX 모드 페이지 (스위처 연결 상태)
 *
 * 6개 페이지:
 * - Page 1: Tally 정보 (PGM/PVW 채널 목록)
 * - Page 2: 스위처 정보 (S1, S2 듀얼 모드 지원)
 * - Page 3: AP (이름, 비밀번호, IP)
 * - Page 4: WIFI (SSID, 비밀번호, IP)
 * - Page 5: ETHERNET (IP, 게이트웨이)
 * - Page 6: 시스템 정보 (3x2 테이블)
 */

#ifndef TX_PAGE_H
#define TX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "DisplayManager.h"
#include "TxPageTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// ========== 페이지 상수 ==========

/**
 * @brief TX 모드 페이지 수
 */
#define TX_PAGE_COUNT 6

/**
 * @brief TxPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool tx_page_init(void);

// ========== Tally 정보 (Page 1) ==========

/**
 * @brief PGM 채널 목록 설정
 * @param channels 채널 배열
 * @param count 채널 수 (최대 20개)
 */
void tx_page_set_pgm_channels(const uint8_t* channels, uint8_t count);

/**
 * @brief PVW 채널 목록 설정
 * @param channels 채널 배열
 * @param count 채널 수 (최대 20개)
 */
void tx_page_set_pvw_channels(const uint8_t* channels, uint8_t count);

// ========== 스위처 정보 (Page 2) ==========

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

// ========== AP 정보 (Page 3) ==========

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

/**
 * @brief AP 활성화 상태 설정
 * @param enabled true=활성화, false=비활성화
 */
void tx_page_set_ap_enabled(bool enabled);

// ========== WIFI 정보 (Page 4) ==========

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

// ========== ETHERNET 정보 (Page 5) ==========

/**
 * @brief Ethernet IP 설정
 * @param ip IP 주소
 */
void tx_page_set_eth_ip(const char* ip);

/**
 * @brief Ethernet DHCP 모드 설정
 * @param dhcp_mode true=DHCP, false=Static
 */
void tx_page_set_eth_dhcp_mode(bool dhcp_mode);

/**
 * @brief Ethernet 연결 상태 설정
 * @param connected 연결 상태
 */
void tx_page_set_eth_connected(bool connected);

/**
 * @brief WiFi 3단계 상태 설정 (신규 API)
 * @param status 네트워크 상태 (NOT_DETECTED/DISCONNECTED/CONNECTED)
 */
void tx_page_set_wifi_status(tx_network_status_t status);

/**
 * @brief Ethernet 3단계 상태 설정 (신규 API)
 * @param status 네트워크 상태 (NOT_DETECTED/DISCONNECTED/CONNECTED)
 */
void tx_page_set_eth_status(tx_network_status_t status);

/**
 * @brief AP 3단계 상태 설정 (신규 API)
 * @param status AP 상태 (INACTIVE/ACTIVE)
 */
void tx_page_set_ap_status(tx_ap_status_t status);

// ========== 시스템 정보 (Page 6) ==========

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
 * @param page 페이지 번호 (1: Tally, 2: Switcher, 3: AP, 4: WIFI, 5: ETHERNET, 6: System)
 */
void tx_page_switch_page(uint8_t page);

/**
 * @brief 현재 페이지 가져오기
 * @return 페이지 번호 (1: Tally, 2: Switcher, 3: AP, 4: WIFI, 5: ETHERNET, 6: System)
 */
uint8_t tx_page_get_current_page(void);

/**
 * @brief 전체 페이지 수 가져오기
 * @return 페이지 수 (TX_PAGE_COUNT)
 */
uint8_t tx_page_get_page_count(void);

#ifdef __cplusplus
}
#endif

#endif // TX_PAGE_H
