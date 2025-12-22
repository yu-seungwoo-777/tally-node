/**
 * @file TallyDispatcher.h
 * @brief Tally 데이터 흐름 제어 (Domain Service Layer)
 *
 * 역할:
 * - TX: Switcher Tally 데이터를 LoRa 패킷으로 변환하여 전송
 * - RX: LoRa 수신 패킷을 각 시스템(DisplayManager, LED 등)에 전파
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "LoRaTypes.h"
#include "esp_err.h"

#ifdef __cplusplus

extern "C" {
    // DisplayManager C 인터페이스
    void DisplayManager_setTallyData(const uint8_t* pgm, uint8_t pgm_count,
                                   const uint8_t* pvw, uint8_t pvw_count);
    void DisplayManager_updateTallyData(const uint8_t* pgm, uint8_t pgm_count,
                                      const uint8_t* pvw, uint8_t pvw_count,
                                      uint8_t total_channels);
}

/**
 * @brief Tally 데이터 흐름 제어
 *
 * 설계 원칙:
 * - 단일 책임: Tally 데이터 흐름 제어만 담당
 * - 실시간성: < 5ms 지연 보장
 * - 저결합도: 다른 컴포넌트와의 의존성 최소화
 */
class TallyDispatcher {
public:
    /**
     * @brief 초기화
     */
    static esp_err_t init();

    /**
     * @brief 주기적 업데이트 (TX 모드)
     *
     * SwitcherManager의 Tally 변경을 감지하고 LoRa 전송
     */
    static void processTallyChanges();

    /**
     * @brief LoRa 수신 패킷 처리 (RX 모드)
     *
     * @param data 수신된 데이터
     * @param length 데이터 길이
     */
    static void onLoRaReceived(const uint8_t* data, size_t length);

    /**
     * @brief 초기화 여부 확인
     */
    static bool isInitialized();

    /**
     * @brief 강제 Tally 업데이트 (매핑 변경 시)
     */
    static void forceUpdate();

#ifdef DEVICE_MODE_TX
    /**
     * @brief 현재 Combined Tally 가져오기
     *
     * @param[out] pgm PGM 채널 배열 (최대 32개)
     * @param[out] pgm_count PGM 채널 수
     * @param[out] pvw PVW 채널 배열 (최대 32개)
     * @param[out] pvw_count PVW 채널 수
     */
    static void getCurrentTally(uint8_t* pgm, uint8_t* pgm_count,
                               uint8_t* pvw, uint8_t* pvw_count);

    /**
     * @brief 스위처 매핑 정보 로그 출력
     */
    static void logMappingInfo();

    /**
     * @brief FastTallyMapper 재초기화 (웹 설정 변경 시)
     */
    static void reinitializeMapper();
#endif

private:
    // 싱글톤 패턴
    TallyDispatcher() = delete;
    ~TallyDispatcher() = delete;
    TallyDispatcher(const TallyDispatcher&) = delete;
    TallyDispatcher& operator=(const TallyDispatcher&) = delete;

    // 내부 구현
    static void onTallyPacketReceived(const uint8_t* data, size_t length);
    static void onConfigChangePacketReceived(const config_change_packet_t* packet);
    static void onStatusPacketReceived(const status_packet_t* packet);

    // Tally 데이터 처리
    static void decodeAndDistributeTally(uint64_t combined_tally, uint8_t channel_count);
    static void updateDisplayData(uint64_t combined_tally, uint8_t channel_count);
    static void updateLedData(uint64_t combined_tally, uint8_t channel_count);

    // 유틸리티
    static void decodeTally(uint64_t packed, uint8_t* pgm, uint8_t* pgm_count,
                           uint8_t* pvw, uint8_t* pvw_count, uint8_t max_channels);
    static void formatTallyString(uint64_t packed, char* buffer, size_t buffer_size,
                                 uint8_t max_channels);

    // 상태 변수
    static bool s_initialized;
#ifdef DEVICE_MODE_TX
    static uint64_t s_last_tally_primary;
    static uint64_t s_last_tally_secondary;
    static uint64_t s_last_combined_tally;
    static uint64_t s_last_tx_time;
#endif
#ifndef DEVICE_MODE_TX
    static uint64_t s_last_rx_time;
#endif
};

// C 래퍼 함수 (외부 C 코드에서 사용)
extern "C" {
    bool TallyDispatcher_isInitialized(void);
    esp_err_t TallyDispatcher_init(void);
    void TallyDispatcher_processTallyChanges(void);
    void TallyDispatcher_onLoRaReceived(const uint8_t* data, size_t length);
    void TallyDispatcher_forceUpdate(void);
}

#endif // __cplusplus