/**
 * @file lora_driver.h
 * @brief LoRa 드라이버 - SX1262/SX1268 제어
 *
 * 04_driver 계층 - 하드웨어 드라이버
 * 05_hal/lora_hal을 사용하여 SPI/GPIO 제어
 */

#ifndef LORA_DRIVER_H
#define LORA_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// LoRa 칩 타입
typedef enum {
    LORA_CHIP_UNKNOWN = 0,
    LORA_CHIP_SX1262_433M = 1,  // SX1262 (433MHz) - 400TB
    LORA_CHIP_SX1268_868M = 2    // SX1268 (868MHz) - 900TB
} lora_chip_type_t;

// LoRa 설정
typedef struct {
    float frequency;            // MHz (기본값: 923.0)
    uint8_t spreading_factor;   // 7-12
    uint8_t coding_rate;        // 5-8 (4/5 ~ 4/8)
    float bandwidth;            // kHz (125, 250, 500)
    int8_t tx_power;            // dBm
    uint8_t sync_word;          // 0x12 (기본값)
} lora_config_t;

// LoRa 상태
typedef struct {
    bool is_initialized;
    lora_chip_type_t chip_type;
    float frequency;            // MHz
    int16_t rssi;               // dBm
    int8_t snr;                 // dB
} lora_status_t;

// 수신 콜백 (RSSI, SNR 포함)
typedef void (*lora_receive_callback_t)(const uint8_t* data, size_t length, int16_t rssi, float snr);

/**
 * @brief LoRa 드라이버 초기화
 *
 * @param config LoRa 설정 (NULL=기본값)
 * @return ESP_OK 성공
 */
esp_err_t lora_driver_init(const lora_config_t* config);

/**
 * @brief LoRa 드라이버 정리
 */
void lora_driver_deinit(void);

/**
 * @brief 상태 가져오기
 */
lora_status_t lora_driver_get_status(void);

/**
 * @brief 칩 이름 가져오기
 */
const char* lora_driver_get_chip_name(void);

/**
 * @brief 패킷 송신 (비동기)
 *
 * @param data 데이터
 * @param length 길이
 * @return ESP_OK 성공
 */
esp_err_t lora_driver_transmit(const uint8_t* data, size_t length);

/**
 * @brief 송신 중 여부 확인
 */
bool lora_driver_is_transmitting(void);

/**
 * @brief 수신 모드 시작
 */
esp_err_t lora_driver_start_receive(void);

/**
 * @brief 수신 콜백 설정
 */
void lora_driver_set_receive_callback(lora_receive_callback_t callback);

/**
 * @brief 수신 체크 (인터럽트 플래그 확인 후 콜백 호출)
 * 메인 루프에서 주기적으로 호출해야 함
 */
void lora_driver_check_received(void);

/**
 * @brief 송신 완료 체크 및 수신 모드 전환
 * 메인 루프에서 주기적으로 호출해야 함
 */
void lora_driver_check_transmitted(void);

/**
 * @brief 절전 모드
 */
esp_err_t lora_driver_sleep(void);

/**
 * @brief 주파수 변경
 */
esp_err_t lora_driver_set_frequency(float freq_mhz);

/**
 * @brief Sync Word 변경
 */
esp_err_t lora_driver_set_sync_word(uint8_t sync_word);

#ifdef __cplusplus
}
#endif

#endif // LORA_DRIVER_H
