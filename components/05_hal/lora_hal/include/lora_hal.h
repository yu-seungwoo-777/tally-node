/**
 * @file lora_hal.h
 * @brief LoRa 전용 HAL - RadioLib를 위한 ESP-IDF SPI/GPIO 추상화
 *
 * LoRa(SX1262/SX1268)를 위한 하드웨어 추상화 계층
 * - SPI2: MOSI=6, MISO=3, SCK=5, CS=7
 * - GPIO: DIO1=33, BUSY=34, RST=8
 */

#ifndef LORA_HAL_H
#define LORA_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
#include <RadioLib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LoRa HAL 초기화
 *
 * SPI2 버스와 GPIO를 초기화합니다.
 * SPI2_HOST: MOSI=6, MISO=3, SCK=5
 *
 * @return ESP_OK 성공, ESP_ERR_NO_MEM 메모리 부족
 */
esp_err_t lora_hal_init(void);

/**
 * @brief LoRa HAL 정리
 */
void lora_hal_deinit(void);

/**
 * @brief SPI 전송
 *
 * @param out 송신 데이터
 * @param in 수신 버퍼 (NULL 가능)
 * @param length 데이터 길이
 * @return ESP_OK 성공
 */
esp_err_t lora_hal_spi_transfer(const uint8_t* out, uint8_t* in, size_t length);

/**
 * @brief 핀 모드 설정
 *
 * @param pin GPIO 핀 번호
 * @param is_input true=입력, false=출력
 */
void lora_hal_pin_mode(uint32_t pin, bool is_input);

/**
 * @brief GPIO 출력
 *
 * @param pin GPIO 핀 번호
 * @param level 0=LOW, 1=HIGH
 */
void lora_hal_digital_write(uint32_t pin, uint32_t level);

/**
 * @brief GPIO 입력
 *
 * @param pin GPIO 핀 번호
 * @return 0=LOW, 1=HIGH
 */
uint32_t lora_hal_digital_read(uint32_t pin);

/**
 * @brief BUSY 핀 대기 (SX126x)
 *
 * @param timeout_us 타임아웃 (마이크로초)
 * @return true=성공(BUSY 해제), false=타임아웃
 */
bool lora_hal_wait_busy(uint32_t timeout_us);

/**
 * @brief 인터럽트 핸들러 등록
 *
 * @param pin GPIO 핀 번호 (DIO1)
 * @param handler 인터럽트 핸들러 함수
 * @return ESP_OK 성공
 */
esp_err_t lora_hal_attach_interrupt(uint32_t pin, void (*handler)(void));

/**
 * @brief 인터럽트 핸들러 제거
 *
 * @param pin GPIO 핀 번호
 */
void lora_hal_detach_interrupt(uint32_t pin);

/**
 * @brief 밀리초 지연
 *
 * @param ms 밀리초
 */
void lora_hal_delay_ms(uint32_t ms);

/**
 * @brief 마이크로초 지연
 *
 * @param us 마이크로초
 */
void lora_hal_delay_us(uint32_t us);

/**
 * @brief 현재 밀리초 가져오기
 *
 * @return 밀리초
 */
uint32_t lora_hal_millis(void);

/**
 * @brief 현재 마이크로초 가져오기
 *
 * @return 마이크로초
 */
uint32_t lora_hal_micros(void);

#ifdef __cplusplus

/**
 * @brief RadioLib HAL 인스턴스 가져오기
 *
 * @return RadioLibHal 포인터
 */
RadioLibHal* lora_hal_get_instance(void);

} // extern "C"

#endif // __cplusplus

#endif // LORA_HAL_H
