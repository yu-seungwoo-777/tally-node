/**
 * @file LoRaCore.h
 * @brief SX1262/SX1268 LoRa Core API
 *
 * Core API 원칙:
 * - 하드웨어 추상화 (SX1262/SX1268 LoRa)
 * - 상태 최소화 (RadioLib 객체 유지)
 * - 단일 책임 (LoRa 송수신)
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "utils.h"
#include "LoRaTypes.h"

/* LoRa 칩 타입 */
enum class LoRaChipType {
    UNKNOWN = 0,
    SX1268_433M = 1,  // SX1268 (433MHz)
    SX1262_868M = 2   // SX1262 (868MHz)
};

/* LoRa 설정 */
struct LoRaConfig {
    uint8_t spreading_factor;   // 확산 팩터 (7-12)
    uint8_t coding_rate;        // 코딩 비율 (5-8)
    float bandwidth;            // 대역폭 (kHz)
    int8_t tx_power;            // 송신 출력 (dBm)
    uint8_t sync_word;          // 동기 워드
    uint16_t preamble_length;   // 프리앰블 길이
};

/* LoRa 상태 (이전 버전 호환) */
struct LoRaStatus {
    bool is_initialized;
    LoRaChipType chip_type;
    float frequency;  // MHz
    float rssi;       // dBm
    float snr;        // dB
    float freq_min;   // 최소 주파수 (MHz)
    float freq_max;   // 최대 주파수 (MHz)
    uint8_t sync_word; // 동기 워드
};

/* 수신 콜백 함수 타입 */
typedef void (*LoRaReceiveCallback)(const uint8_t* data, size_t length);

/* 채널 스캔 정보 (이전 버전 호환) */
struct ChannelInfo {
    float frequency;  // 주파수 (MHz)
    float rssi;       // RSSI (dBm)
    bool available;   // 사용 가능 여부 (RSSI < -100dBm)
};

/**
 * @brief LoRa Core API
 *
 * 설계 원칙:
 * - 상태: RadioLib 객체, 칩 정보만 유지
 * - 스레드 안전성: 단일 스레드 사용 권장
 * - 성능: Cold Path (초기화), Hot Path (송수신 가능하지만 일반적으로 low rate)
 */
class LoRaCore {
public:
    /**
     * @brief 초기화 및 칩 자동 감지
     *
     * SX1262(868MHz) 또는 SX1268(433MHz)를 자동 감지하고 초기화합니다.
     *
     * @param config LoRa 설정 (nullptr이면 기본값 사용)
     */
    static esp_err_t init(const LoRaConfig* config = nullptr);

    /**
     * @brief LoRa 상태 가져오기
     */
    static lora_status_t getStatus();

    /**
     * @brief 칩 타입 이름 가져오기
     */
    static const char* getChipName();

    /**
     * @brief LoRa 패킷 송신 (동기)
     *
     * @param data 송신 데이터
     * @param length 데이터 길이
     */
    static esp_err_t transmit(const uint8_t* data, size_t length);

    /**
     * @brief LoRa 패킷 송신 (비동기)
     *
     * @param data 송신 데이터
     * @param length 데이터 길이
     */
    static esp_err_t transmitAsync(const uint8_t* data, size_t length);

    /**
     * @brief 송신 진행 중 여부 확인
     */
    static bool isTransmitting();

    /**
     * @brief 수신 모드 시작
     */
    static esp_err_t startReceive();

    /**
     * @brief 수신 콜백 함수 등록
     *
     * @param callback 수신 콜백 함수
     */
    static void setReceiveCallback(LoRaReceiveCallback callback);

    /**
     * @brief 수신 체크 및 콜백 호출
     *
     * 인터럽트 플래그를 확인하고 수신된 데이터가 있으면 콜백을 호출합니다.
     * 메인 루프에서 주기적으로 호출해야 합니다.
     */
    static void checkReceived();

    /**
     * @brief 송신 완료 체크 및 수신 모드 전환
     *
     * 인터럽트 플래그를 확인하고 송신이 완료되었으면 startReceive()를 호출합니다.
     * 메인 루프에서 주기적으로 호출해야 합니다.
     */
    static void checkTransmitted();

    /**
     * @brief 절전 모드 진입
     */
    static esp_err_t sleep();

    /**
     * @brief 주파수 채널 스캔
     *
     * 지정된 주파수 범위를 스캔하여 각 채널의 RSSI를 측정합니다.
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

private:
    // 싱글톤 패턴
    LoRaCore() = delete;
    ~LoRaCore() = delete;
    LoRaCore(const LoRaCore&) = delete;
    LoRaCore& operator=(const LoRaCore&) = delete;

    // 내부 구현은 cpp 파일에서 정의
    // (RadioLib 헤더를 여기서 include하지 않음 - 컴파일 속도 향상)
};
