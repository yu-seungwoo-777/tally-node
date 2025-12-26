/**
 * @file web_server.h
 * @brief Web Server for Tally Node Control Interface
 */

#ifndef TALLY_WEB_SERVER_H
#define TALLY_WEB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief 웹 서버 초기화
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_init(void);

/**
 * @brief 웹 서버 시작
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_start(void);

/**
 * @brief 웹 서버 중지
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t web_server_stop(void);

/**
 * @brief 웹 서버 상태 확인
 * @return true 실행 중, false 중지됨
 */
bool web_server_is_running(void);

/**
 * @brief 모든 WebSocket 클라이언트에 Tally 상태 브로드캐스트
 * @param channels 채널 상태 배열
 * @param count 채널 수
 */
void web_server_broadcast_tally(const uint8_t *channels, size_t count);

/**
 * @brief LoRa 상태 브로드캐스트
 * @param rssi RSSI 값 (dBm)
 * @param snr SNR 값 (dB)
 * @param tx_packets 송신 패킷 수
 * @param rx_packets 수신 패킷 수
 */
void web_server_broadcast_lora(int16_t rssi, int8_t snr, uint32_t tx_packets, uint32_t rx_packets);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_H
