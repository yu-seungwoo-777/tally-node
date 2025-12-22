/**
 * @file LoRaPacket.h
 * @brief LoRa 패킷 프로토콜 정의
 *
 * 패킷 프로토콜:
 * - Tally 패킷 (TX → RX): [0xAA][Channel Count][Combined Tally (가변)]
 *   - 채널당 2비트 사용 (Program=0b10, Preview=0b01)
 *   - 필요 바이트 = (채널 수 + 3) / 4
 *   - 예: 4채널=1바이트, 8채널=2바이트, 12채널=3바이트, 20채널=5바이트
 * - Status 패킷 (RX → TX): [0xBB][RX ID][Battery][RSSI]... (미구현)
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "LoRaTypes.h"

/* 패킷 타입 (이전 버전 호환) */
enum class LoRaPacketType : uint8_t {
    TALLY = 0xAA,       // Tally 패킷 (TX → RX broadcast) - 이전 버전
    STATUS = 0xBB,      // 상태 보고 패킷 (RX → TX)
    CONFIG_CHANGE = 0xFC, // 설정 변경 패킷 (TX → RX)
    UNKNOWN = 0xFF      // 알 수 없는 패킷
};

/* 새로운 Tally 패킷 헤더 (F1-F4) */
#define TALLY_PACKET_8CH_HEADER   0xF1  // 8채널 Tally 패킷 (3바이트)
#define TALLY_PACKET_12CH_HEADER  0xF2  // 12채널 Tally 패킷 (4바이트)
#define TALLY_PACKET_16CH_HEADER  0xF3  // 16채널 Tally 패킷 (5바이트)
#define TALLY_PACKET_20CH_HEADER  0xF4  // 20채널 Tally 패킷 (6바이트)

/* Tally 패킷 헤더 (이전 버전 호환) */
struct TallyPacketHeader {
    uint8_t header;           // 0xAA
    uint8_t channel_count;    // 채널 수 (0 = Heartbeat)
    // 이후 가변 길이 tally data (채널 수에 따라 1~8바이트)
} __attribute__((packed));

/* 최대 Tally 패킷 구조 (32채널) (이전 버전 호환) */
struct TallyPacket {
    uint8_t header;           // 0xAA
    uint8_t channel_count;    // 채널 수 (0 = Heartbeat)
    uint64_t combined_tally;  // Combined Tally (최대 8바이트)
} __attribute__((packed));

/* Status 패킷 구조 (이전 버전 호환) */
struct StatusPacket {
    uint8_t header;     // 0xBB
    uint8_t rx_id;      // RX 장치 ID
    uint8_t battery;    // 배터리 레벨 (%)
    int8_t rssi;        // RSSI (dBm)
} __attribute__((packed));

/* Config Change 패킷 구조 (이전 버전 호환) */
struct ConfigChangePacket {
    uint8_t header;     // 0xFC
    float frequency;    // 새 주파수 (MHz)
    uint8_t sync_word;  // 새 Sync Word
} __attribute__((packed));

/* 패킷 크기 상수 */
constexpr size_t TALLY_PACKET_HEADER_SIZE = sizeof(TallyPacketHeader);
constexpr size_t TALLY_PACKET_MAX_SIZE = sizeof(TallyPacket);
constexpr size_t STATUS_PACKET_SIZE = sizeof(StatusPacket);
constexpr size_t CONFIG_CHANGE_PACKET_SIZE = sizeof(ConfigChangePacket);

/**
 * @brief 채널 수에 따른 Tally 데이터 바이트 수 계산
 * @param channel_count 채널 수
 * @return 필요한 바이트 수 (1~8)
 */
constexpr size_t getTallyDataSize(uint8_t channel_count) {
    return (channel_count > 0) ? ((channel_count + 3) / 4) : 0;
}

/**
 * @brief 채널 수에 따른 Tally 패킷 전체 크기 계산
 * @param channel_count 채널 수
 * @return 패킷 크기 (헤더 2바이트 + 데이터)
 */
constexpr size_t getTallyPacketSize(uint8_t channel_count) {
    return TALLY_PACKET_HEADER_SIZE + getTallyDataSize(channel_count);
}

/**
 * @brief LoRa 패킷 API
 */
namespace LoRaPacket {

/**
 * @brief 패킷 타입 확인
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @return 패킷 타입
 */
LoRaPacketType getPacketType(const uint8_t* data, size_t length);

/**
 * @brief Tally 패킷 생성 (동적 크기)
 *
 * @param combined_tally Combined Tally 데이터 (최대 32채널, 2비트씩)
 * @param channel_count 채널 수 (1~32)
 * @param out_buffer 출력 버퍼 (최소 getTallyPacketSize(channel_count) 바이트)
 * @param buffer_size 버퍼 크기
 * @return 패킷 크기 (성공 시 2~10바이트, 실패 시 0)
 */
size_t createTallyPacket(uint64_t combined_tally, uint8_t channel_count,
                         uint8_t* out_buffer, size_t buffer_size);

/**
 * @brief Heartbeat 패킷 생성
 *
 * Heartbeat는 channel_count=0인 Tally 패킷 (헤더만 2바이트)
 *
 * @param out_buffer 출력 버퍼 (최소 2바이트)
 * @param buffer_size 버퍼 크기
 * @return 패킷 크기 (성공 시 2, 실패 시 0)
 */
size_t createHeartbeatPacket(uint8_t* out_buffer, size_t buffer_size);

/**
 * @brief Tally 패킷 파싱
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @param out_combined_tally Combined Tally 출력 (nullable)
 * @param out_channel_count 채널 수 출력 (nullable)
 * @return ESP_OK: 성공, ESP_FAIL: 실패
 */
esp_err_t parseTallyPacket(const uint8_t* data, size_t length,
                           uint64_t* out_combined_tally,
                           uint8_t* out_channel_count);

/**
 * @brief Tally 패킷인지 확인
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @return true: Tally 패킷, false: 아님
 */
bool isTallyPacket(const uint8_t* data, size_t length);

/**
 * @brief Heartbeat 패킷인지 확인
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @return true: Heartbeat 패킷, false: 아님
 */
bool isHeartbeatPacket(const uint8_t* data, size_t length);

/**
 * @brief Config Change 패킷 생성
 *
 * @param frequency 새 주파수 (MHz)
 * @param sync_word 새 Sync Word
 * @param out_buffer 출력 버퍼 (최소 6바이트)
 * @param buffer_size 버퍼 크기
 * @return 패킷 크기 (성공 시 6, 실패 시 0)
 */
size_t createConfigChangePacket(float frequency, uint8_t sync_word,
                                uint8_t* out_buffer, size_t buffer_size);

/**
 * @brief Config Change 패킷 파싱
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @param out_frequency 새 주파수 출력 (nullable)
 * @param out_sync_word 새 Sync Word 출력 (nullable)
 * @return ESP_OK: 성공, ESP_FAIL: 실패
 */
esp_err_t parseConfigChangePacket(const uint8_t* data, size_t length,
                                  float* out_frequency, uint8_t* out_sync_word);

/**
 * @brief Config Change 패킷인지 확인
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @return true: Config Change 패킷, false: 아님
 */
bool isConfigChangePacket(const uint8_t* data, size_t length);

/**
 * @brief 새로운 Tally 패킷 파싱 (F1-F4 헤더)
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @param out_combined_tally Combined Tally 출력 (nullable)
 * @param out_channel_count 채널 수 출력 (nullable)
 * @return ESP_OK: 성공, ESP_FAIL: 실패
 */
esp_err_t parseNewTallyPacket(const uint8_t* data, size_t length,
                             uint64_t* out_combined_tally,
                             uint8_t* out_channel_count);

/**
 * @brief 새로운 Tally 패킷인지 확인 (F1-F4)
 *
 * @param data 패킷 데이터
 * @param length 패킷 길이
 * @return true: 새로운 Tally 패킷, false: 아님
 */
bool isNewTallyPacket(const uint8_t* data, size_t length);

/**
 * @brief 패킷 헤더에 따른 채널 수 가져오기
 *
 * @param header 패킷 헤더 (F1-F4)
 * @return 채널 수 (0 = 잘못된 헤더)
 */
uint8_t getChannelCountFromHeader(uint8_t header);

/**
 * @brief 패킷 헤더에 따른 데이터 길이 가져오기
 *
 * @param header 패킷 헤더 (F1-F4)
 * @return 데이터 바이트 수 (0 = 잘못된 헤더)
 */
uint8_t getDataLengthFromHeader(uint8_t header);

} // namespace LoRaPacket
