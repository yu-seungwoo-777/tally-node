/**
 * @file HardwareService.h
 * @brief 하드웨어 정보 수집 서비스
 *
 * 역할: 하드웨어 상태 정보 수집
 * - 배터리 (ADC)
 * - 전압
 * - 온도 (내부 센서)
 * - RSSI/SNR (LoRa)
 * - 업타임
 * - Device ID (EFUSE)
 */

#ifndef HARDWARE_SERVICE_H
#define HARDWARE_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 시스템 상태 구조체
// ============================================================================

// event_bus.h의 system_info_event_t를 그대로 사용
#define hardware_system_t system_info_event_t

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief HardwareService 초기화
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t hardware_service_init(void);

/**
 * @brief HardwareService 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t hardware_service_deinit(void);

/**
 * @brief 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool hardware_service_is_initialized(void);

// ============================================================================
// 태스크 제어
// ============================================================================

/**
 * @brief 모니터링 태스크 시작 (1초 주기)
 * @return ESP_OK 성공
 */
esp_err_t hardware_service_start(void);

/**
 * @brief 모니터링 태스크 정지
 * @return ESP_OK 성공
 */
esp_err_t hardware_service_stop(void);

/**
 * @brief 태스크 실행 중 여부
 * @return true 실행 중, false 정지됨
 */
bool hardware_service_is_running(void);

// ============================================================================
// Device ID
// ============================================================================

/**
 * @brief Device ID 가져오기 (4자리 hex 문자열)
 * @return Device ID (MAC 기반, 읽기 전용)
 */
const char* hardware_service_get_device_id(void);

// ============================================================================
// 배터리/전압
// ============================================================================

/**
 * @brief 배터리 잔량 설정 (퍼센트)
 * @param battery 배터리 % (0-100)
 */
void hardware_service_set_battery(uint8_t battery);

/**
 * @brief 배터리 ADC로 읽기 (백분율 반환)
 * @return 배터리 % (0-100)
 */
uint8_t hardware_service_update_battery(void);

/**
 * @brief 배터리 잔량 가져오기
 * @return 배터리 % (0-100)
 */
uint8_t hardware_service_get_battery(void);

/**
 * @brief 전압 가져오기
 * @return 전압 (V)
 */
float hardware_service_get_voltage(void);

// ============================================================================
// 온도
// ============================================================================

/**
 * @brief 온도 가져오기
 * @return 온도 (°C)
 */
float hardware_service_get_temperature(void);

// ============================================================================
// RSSI/SNR (LoRa)
// ============================================================================

/**
 * @brief RSSI 설정 (LoRa 이벤트 수신 후 호출)
 * @param rssi RSSI (dBm)
 */
void hardware_service_set_rssi(int16_t rssi);

/**
 * @brief RSSI 가져오기
 * @return RSSI (dBm)
 */
int16_t hardware_service_get_rssi(void);

/**
 * @brief SNR 설정 (LoRa 이벤트 수신 후 호출)
 * @param snr SNR (dB)
 */
void hardware_service_set_snr(float snr);

/**
 * @brief SNR 가져오기
 * @return SNR (dB)
 */
float hardware_service_get_snr(void);

// ============================================================================
// 업타임/상태
// ============================================================================

/**
 * @brief Stopped 상태 설정
 * @param stopped true=정지, false=동작
 */
void hardware_service_set_stopped(bool stopped);

/**
 * @brief Stopped 상태 가져오기
 * @return true=정지, false=동작
 */
bool hardware_service_get_stopped(void);

/**
 * @brief 업타임 증가 (1초 타이머 호출용)
 */
void hardware_service_inc_uptime(void);

/**
 * @brief 업타임 가져오기
 * @return 업타임 (초)
 */
uint32_t hardware_service_get_uptime(void);

// ============================================================================
// 전체 상태
// ============================================================================

/**
 * @brief 전체 시스템 상태 가져오기
 * @param status 상태 구조체 포인터
 */
void hardware_service_get_system(hardware_system_t* status);

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_SERVICE_H
