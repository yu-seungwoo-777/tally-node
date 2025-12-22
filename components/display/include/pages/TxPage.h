/**
 * @file TxPage.h
 * @brief TALLY-NODE TX Mode Page
 *
 * TX 모드에서 표시되는 페이지 관리
 * - Page 1: 스위처 연결 정보
 * - Page 2: 네트워크 설정 정보
 * - Page 3: 시스템 정보
 */

#ifndef TX_PAGE_H
#define TX_PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TX 페이지 초기화
 *
 * @return esp_err_t ESP_OK 성공, 그 외 에러
 */
esp_err_t TxPage_init(void);

/**
 * @brief TX 페이지 표시
 */
void TxPage_showPage(void);

/**
 * @brief TX 페이지 숨기기
 */
void TxPage_hidePage(void);

/**
 * @brief 페이지 전환
 *
 * @param page 페이지 번호 (1: Switcher, 2: Network, 3: System)
 */
void TxPage_switchPage(uint8_t page);

/**
 * @brief 현재 페이지 가져오기
 *
 * @return uint8_t 현재 페이지 번호
 */
uint8_t TxPage_getCurrentPage(void);

/**
 * @brief 강제 업데이트
 */
void TxPage_forceUpdate(void);

/**
 * @brief 네트워크 상태 업데이트
 *
 * @param connected true: 연결됨, false: 연결 안됨
 * @param ip IP 주소 문자열 (NULL 가능)
 */
void TxPage_setNetworkStatus(bool connected, const char* ip);

/**
 * @brief LoRa 전송 상태 업데이트
 *
 * @param transmitting true: 전송 중, false: 대기 중
 */
void TxPage_setLoRaStatus(bool transmitting);

/**
 * @brief 스위처 연결 상태 업데이트
 *
 * @param connected true: 연결됨, false: 연결 안됨
 */
void TxPage_setSwitcherStatus(bool connected);

#ifdef __cplusplus
}
#endif

#endif // TX_PAGE_H