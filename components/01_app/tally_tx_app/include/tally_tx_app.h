/**
 * @file tally_tx_app.h
 * @brief Tally 송신 앱 (Application Layer)
 *
 * 역할: 스위처 연결 및 LoRa Tally 송신
 * - SwitcherService를 통해 스위처 연결
 * - LoRaService를 통해 Tally 데이터 송신
 * - 듀얼모드 지원
 */

#ifndef TALLY_TX_APP_H
#define TALLY_TX_APP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tally 송신 앱 설정
 */
typedef struct {
    const char* switcher1_ip;       ///< Primary 스위처 IP
    const char* switcher2_ip;       ///< Secondary 스위처 IP (nullptr = 미사용)
    uint16_t switcher_port;         ///< 스위처 포트 (0 = 기본값 9910)
    uint8_t camera_limit;           ///< 카메라 제한 (0 = 자동)
    bool dual_mode;                 ///< 듀얼모드 활성화
    uint8_t secondary_offset;       ///< Secondary 오프셋
    uint32_t send_interval_ms;      ///< LoRa 송신 간격 (ms)
    uint8_t switcher1_interface;    ///< Primary 네트워크 인터페이스 (1=WiFi, 2=Ethernet, 0=자동)
    uint8_t switcher2_interface;    ///< Secondary 네트워크 인터페이스 (1=WiFi, 2=Ethernet, 0=자동)
} tally_tx_config_t;

/**
 * @brief 기본 설정
 */
extern const tally_tx_config_t TALLY_TX_DEFAULT_CONFIG;

/**
 * @brief 앱 초기화
 * @param config 설정 (nullptr = 기본값 사용)
 * @return 성공 여부
 */
bool tally_tx_app_init(const tally_tx_config_t* config);

/**
 * @brief 앱 시작
 */
void tally_tx_app_start(void);

/**
 * @brief 앱 정지
 */
void tally_tx_app_stop(void);

/**
 * @brief 앱 정리
 */
void tally_tx_app_deinit(void);

/**
 * @brief 루프 처리 (주기적으로 호출)
 */
void tally_tx_app_loop(void);

/**
 * @brief 상태 출력
 */
void tally_tx_app_print_status(void);

/**
 * @brief 연결 상태 조회
 * @return 모든 스위처가 연결되었으면 true
 */
bool tally_tx_app_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // TALLY_TX_APP_H
