/**
 * @file tally_rx_app.h
 * @brief Tally 수신 앱 (Application Layer)
 *
 * 역할: LoRa Tally 데이터 수신
 * - LoRaService를 통해 Tally 데이터 수신
 * - 수신된 Tally 데이터를 해석 및 로그 출력
 * - 디스플레이 출력 (예정)
 */

#ifndef TALLY_RX_APP_H
#define TALLY_RX_APP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tally 수신 앱 설정
 */
typedef struct {
    // LoRa 설정
    float frequency;          ///< LoRa 주파수 (MHz)
    uint8_t spreading_factor; ///< Spreading Factor (7-12)
    uint8_t coding_rate;      ///< Coding Rate (5-8)
    float bandwidth;          ///< Bandwidth (kHz)
    int8_t tx_power;          ///< 송신 전력 (dBm)
    uint8_t sync_word;        ///< Sync Word
} tally_rx_config_t;

/**
 * @brief 기본 설정
 */
extern const tally_rx_config_t TALLY_RX_DEFAULT_CONFIG;

/**
 * @brief 앱 초기화
 * @param config 설정 (nullptr = 기본값 사용)
 * @return 성공 여부
 */
bool tally_rx_app_init(const tally_rx_config_t* config);

/**
 * @brief 앱 시작
 */
void tally_rx_app_start(void);

/**
 * @brief 앱 정지
 */
void tally_rx_app_stop(void);

/**
 * @brief 앱 정리
 */
void tally_rx_app_deinit(void);

/**
 * @brief 루프 처리 (주기적으로 호출)
 */
void tally_rx_app_loop(void);

/**
 * @brief 상태 출력
 */
void tally_rx_app_print_status(void);

/**
 * @brief 실행 중 여부 조회
 */
bool tally_rx_app_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // TALLY_RX_APP_H
