/**
 * @file LoRaManager.h
 * @brief LoRa 통신 관리 Manager
 *
 * Manager 역할:
 * - LoRaCore 초기화 및 관리
 * - 송신/수신 API 제공
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "LoRaTypes.h"
#include "LoRaCore.h"

/**
 * @brief LoRa 통신 관리 Manager
 *
 * 설계 원칙:
 * - LoRaCore를 래핑하여 간단한 API 제공
 * - 수신 콜백 등록
 */
class LoRaManager {
public:
    /**
     * @brief 초기화
     *
     * @param config LoRa 설정 (nullptr이면 기본값)
     */
    static esp_err_t init(const LoRaConfig* config = nullptr);

    /**
     * @brief 데이터 송신 (동기)
     *
     * @param data 송신할 데이터
     * @param length 데이터 길이
     * @return 성공 시 ESP_OK
     */
    static esp_err_t transmit(const uint8_t* data, size_t length);

    /**
     * @brief 데이터 송신 (비동기)
     *
     * @param data 송신할 데이터
     * @param length 데이터 길이
     * @return 성공 시 ESP_OK
     */
    static esp_err_t transmitAsync(const uint8_t* data, size_t length);

    /**
     * @brief 수신 시작
     */
    static esp_err_t startReceive();

    /**
     * @brief 수신 콜백 설정
     *
     * @param callback 수신 콜백 함수
     */
    static void setReceiveCallback(LoRaReceiveCallback callback);

    /**
     * @brief 수신 체크 (루프에서 호출)
     */
    static void checkReceived();

    /**
     * @brief 송신 완료 체크 (루프에서 호출)
     */
    static void checkTransmitted();

    /**
     * @brief 송신 중인지 확인
     */
    static bool isTransmitting();

    /**
     * @brief LoRa 상태 가져오기
     */
    static lora_status_t getStatus();

    /**
     * @brief 초기화 여부 확인
     */
    static bool isInitialized();

    /**
     * @brief 주파수 채널 스캔
     *
     * @param start_freq 시작 주파수 (MHz)
     * @param end_freq 종료 주파수 (MHz)
     * @param step 스캔 간격 (MHz, 권장: 0.1~0.5)
     * @param results 결과 배열 (출력)
     * @param max_results 결과 배열 최대 크기
     * @param result_count 실제 스캔된 채널 수 (출력)
     * @return ESP_OK: 성공, ESP_FAIL: 실패
     */
    static esp_err_t scanChannels(float start_freq, float end_freq, float step,
                                   channel_info_t* results, size_t max_results,
                                   size_t* result_count);

    /**
     * @brief 주파수 변경
     *
     * @param freq 주파수 (MHz)
     * @return ESP_OK: 성공, ESP_FAIL: 실패
     */
    static esp_err_t setFrequency(float freq);

    /**
     * @brief Sync Word 변경
     *
     * @param sync_word 동기 워드 (0x00~0xFF)
     * @return ESP_OK: 성공, ESP_FAIL: 실패
     */
    static esp_err_t setSyncWord(uint8_t sync_word);

    /**
     * @brief 주파수 임시 저장 (실제 적용 안 함)
     *
     * @param freq 주파수 (MHz)
     */
    static void setPendingFrequency(float freq);

    /**
     * @brief Sync Word 임시 저장 (실제 적용 안 함)
     *
     * @param sync_word 동기 워드 (0x00~0xFF)
     */
    static void setPendingSyncWord(uint8_t sync_word);

    /**
     * @brief 임시 저장된 설정이 있는지 확인
     *
     * @return true: 임시 저장된 설정 있음, false: 없음
     */
    static bool hasPendingConfig();

    /**
     * @brief 임시 저장된 설정 적용
     *
     * 1초 간격으로 3회 Config Change 패킷 전송 후
     * 1초 대기 후 TX 설정 변경
     *
     * @return ESP_OK: 성공, ESP_FAIL: 실패
     */
    static esp_err_t applyPendingConfig();

private:
    // 싱글톤 패턴
    LoRaManager() = delete;
    ~LoRaManager() = delete;
    LoRaManager(const LoRaManager&) = delete;
    LoRaManager& operator=(const LoRaManager&) = delete;

    static bool s_initialized;
};
