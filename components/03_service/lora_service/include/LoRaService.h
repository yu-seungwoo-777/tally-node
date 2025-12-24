/**
 * @file LoRaService.h
 * @brief LoRa Service - LoRa 통신 관리 서비스
 *
 * 03_service 계층 - 비즈니스 로직
 * - 04_driver/lora_driver를 사용하여 LoRa 통신 관리
 * - 내부 FreeRTOS 태스크로 송신 큐 처리
 * - event_bus로 이벤트 발행
 */

#ifndef LORA_SERVICE_H
#define LORA_SERVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "TallyTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// LoRa 칩 타입 (driver와 동일)
typedef enum {
    LORA_SERVICE_CHIP_UNKNOWN = 0,
    LORA_SERVICE_CHIP_SX1262 = 1,   // 868/915MHz
    LORA_SERVICE_CHIP_SX1268 = 2    // 433MHz
} lora_service_chip_type_t;

// LoRa Service 설정
typedef struct {
    float frequency;            // MHz
    uint8_t spreading_factor;   // 7-12
    uint8_t coding_rate;        // 5-8
    float bandwidth;            // kHz
    int8_t tx_power;            // dBm
    uint8_t sync_word;          // 0x12
} lora_service_config_t;

// LoRa Service 상태
typedef struct {
    bool is_running;
    bool is_initialized;
    lora_service_chip_type_t chip_type;
    float frequency;            // MHz
    int16_t rssi;               // dBm
    int8_t snr;                 // dB
    uint32_t packets_sent;      // 송신 패킷 수
    uint32_t packets_received;  // 수신 패킷 수
} lora_service_status_t;

// 수신 콜백
typedef void (*lora_service_receive_callback_t)(const uint8_t* data, size_t length);

/**
 * @brief LoRa Service 초기화
 *
 * @param config 설정 (NULL=기본값)
 * @return ESP_OK 성공
 */
esp_err_t lora_service_init(const lora_service_config_t* config);

/**
 * @brief LoRa Service 시작 (내부 태스크 시작)
 *
 * @return ESP_OK 성공
 */
esp_err_t lora_service_start(void);

/**
 * @brief LoRa Service 정지
 */
void lora_service_stop(void);

/**
 * @brief LoRa Service 해제
 */
void lora_service_deinit(void);

/**
 * @brief 데이터 송신
 *
 * @param data 데이터
 * @param length 길이
 * @return ESP_OK 성공
 */
esp_err_t lora_service_send(const uint8_t* data, size_t length);

/**
 * @brief 문자열 송신 (유틸리티)
 *
 * @param str null로 끝나는 문자열
 * @return ESP_OK 성공
 */
esp_err_t lora_service_send_string(const char* str);

/**
 * @brief Tally 데이터 송신 (F1-F4 헤더 형식)
 *
 * @param tally Packed Tally 데이터
 * @return ESP_OK 성공
 */
esp_err_t lora_service_send_tally(const packed_data_t* tally);

// ============================================================================
// Tally 패킷 해석 (수신)
// ============================================================================

/**
 * @brief Tally 패킷 헤더에서 채널 수 추출
 *
 * @param header 패킷 헤더 (0xF1~0xF4)
 * @return 채널 수 (0=잘못된 헤더)
 */
uint8_t lora_service_tally_get_channel_count(uint8_t header);

/**
 * @brief 수신 콜백 설정
 *
 * @param callback 콜백 함수
 */
void lora_service_set_receive_callback(lora_service_receive_callback_t callback);

/**
 * @brief 상태 가져오기
 *
 * @return 상태 구조체
 */
lora_service_status_t lora_service_get_status(void);

/**
 * @brief 실행 중 여부 확인
 *
 * @return true 실행 중
 */
bool lora_service_is_running(void);

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨
 */
bool lora_service_is_initialized(void);

/**
 * @brief 주파수 변경
 *
 * @param freq_mhz 주파수 (MHz)
 * @return ESP_OK 성공
 */
esp_err_t lora_service_set_frequency(float freq_mhz);

/**
 * @brief Sync Word 변경
 *
 * @param sync_word 동기 워드
 * @return ESP_OK 성공
 */
esp_err_t lora_service_set_sync_word(uint8_t sync_word);

#ifdef __cplusplus
}
#endif

#endif // LORA_SERVICE_H
