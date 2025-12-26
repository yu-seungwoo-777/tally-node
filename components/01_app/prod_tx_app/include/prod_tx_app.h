/**
 * @file prod_tx_app.h
 * @brief 프로덕션 Tally 송신 앱
 */

#ifndef PROD_TX_APP_H
#define PROD_TX_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 송신 앱 설정 구조체
 */
typedef struct {
    uint32_t frequency;        ///< LoRa 주파수 (Hz)
    uint8_t spreading_factor;  ///< Spreading Factor (7-12)
    uint8_t coding_rate;       ///< Coding Rate (4/5=1, 4/6=2, 4/7=3, 4/8=4)
    uint32_t bandwidth;        ///< 대역폭 (Hz)
    int8_t tx_power;           ///< 송신 전력 (dBm)
    uint8_t sync_word;         ///< Sync Word
} prod_tx_config_t;

/**
 * @brief 송신 앱 초기화
 *
 * @param config 설정 (NULL 시 기본값 사용)
 * @return true 성공, false 실패
 */
bool prod_tx_app_init(const prod_tx_config_t* config);

/**
 * @brief 송신 앱 시작
 */
void prod_tx_app_start(void);

/**
 * @brief 송신 앱 정지
 */
void prod_tx_app_stop(void);

/**
 * @brief 송신 앱 정리
 */
void prod_tx_app_deinit(void);

/**
 * @brief 송신 앱 루프 (메인 루프에서 호출)
 */
void prod_tx_app_loop(void);

/**
 * @brief 송신 앱 상태 출력
 */
void prod_tx_app_print_status(void);

/**
 * @brief 송신 앱 실행 중 여부
 *
 * @return true 실행 중, false 정지됨
 */
bool prod_tx_app_is_running(void);

/**
 * @brief 스위처 연결 상태 확인
 *
 * @return true 연결됨, false 연결 안됨
 */
bool prod_tx_app_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // PROD_TX_APP_H
