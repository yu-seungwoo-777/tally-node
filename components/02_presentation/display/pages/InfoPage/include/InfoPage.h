/**
 * @file InfoPage.h
 * @brief 정보 표시 페이지
 */

#ifndef INFO_PAGE_H
#define INFO_PAGE_H

#include "DisplayManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief InfoPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool info_page_init(void);

/**
 * @brief IP 주소 설정
 * @param ip IP 주소 문자열
 */
void info_page_set_ip(const char* ip);

/**
 * @brief 배터리 퍼센트 설정
 * @param percent 배터리 퍼센트 (0-100)
 */
void info_page_set_battery(uint8_t percent);

/**
 * @brief LoRa RSSI 설정
 * @param rssi RSSI 값 (dBm)
 */
void info_page_set_rssi(int16_t rssi);

/**
 * @brief LoRa SNR 설정
 * @param snr SNR 값 (dB)
 */
void info_page_set_snr(int8_t snr);

/**
 * @brief 연결 상태 설정
 * @param connected true = 연결됨, false = 연결 안됨
 */
void info_page_set_connection(bool connected);

/**
 * @brief 업타임 설정
 * @param seconds 업타임 (초)
 */
void info_page_set_uptime(uint32_t seconds);

#ifdef __cplusplus
}
#endif

#endif // INFO_PAGE_H
