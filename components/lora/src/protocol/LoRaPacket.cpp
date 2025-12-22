/**
 * @file LoRaPacket.cpp
 * @brief LoRa 패킷 프로토콜 구현
 */

#include "LoRaPacket.h"
#include <string.h>

namespace LoRaPacket {

LoRaPacketType getPacketType(const uint8_t* data, size_t length)
{
    if (!data || length < 1) {
        return LoRaPacketType::UNKNOWN;
    }

    uint8_t header = data[0];

    switch (header) {
        case static_cast<uint8_t>(LoRaPacketType::TALLY):
            return LoRaPacketType::TALLY;
        case static_cast<uint8_t>(LoRaPacketType::STATUS):
            return LoRaPacketType::STATUS;
        case static_cast<uint8_t>(LoRaPacketType::CONFIG_CHANGE):
            return LoRaPacketType::CONFIG_CHANGE;
        default:
            return LoRaPacketType::UNKNOWN;
    }
}

size_t createTallyPacket(uint64_t combined_tally, uint8_t channel_count,
                         uint8_t* out_buffer, size_t buffer_size)
{
    // 필요한 패킷 크기 계산
    size_t required_size = getTallyPacketSize(channel_count);

    if (!out_buffer || buffer_size < required_size) {
        return 0;
    }

    // 헤더 작성
    out_buffer[0] = static_cast<uint8_t>(LoRaPacketType::TALLY);
    out_buffer[1] = channel_count;

    // 채널 수에 따른 데이터만 복사 (바이트 순서 명시적)
    if (channel_count > 0) {
        size_t data_size = getTallyDataSize(channel_count);
        const uint8_t* src = (const uint8_t*)&combined_tally;
        // little-endian 방식으로 바이트 복사
        for (size_t i = 0; i < data_size; i++) {
            out_buffer[TALLY_PACKET_HEADER_SIZE + i] = src[i];
        }
    }

    return required_size;
}

size_t createHeartbeatPacket(uint8_t* out_buffer, size_t buffer_size)
{
    // Heartbeat는 channel_count=0인 Tally 패킷 (헤더만 2바이트)
    if (!out_buffer || buffer_size < TALLY_PACKET_HEADER_SIZE) {
        return 0;
    }

    out_buffer[0] = static_cast<uint8_t>(LoRaPacketType::TALLY);
    out_buffer[1] = 0;  // channel_count = 0

    return TALLY_PACKET_HEADER_SIZE;
}

esp_err_t parseTallyPacket(const uint8_t* data, size_t length,
                           uint64_t* out_combined_tally,
                           uint8_t* out_channel_count)
{
    // 최소 헤더 크기 확인
    if (!data || length < TALLY_PACKET_HEADER_SIZE) {
        return ESP_FAIL;
    }

    if (data[0] != static_cast<uint8_t>(LoRaPacketType::TALLY)) {
        return ESP_FAIL;
    }

    uint8_t channel_count = data[1];

    // 채널 수에 맞는 패킷 크기 확인
    size_t expected_size = getTallyPacketSize(channel_count);
    if (length < expected_size) {
        return ESP_FAIL;
    }

    if (out_channel_count) {
        *out_channel_count = channel_count;
    }

    if (out_combined_tally) {
        *out_combined_tally = 0;  // 초기화
        if (channel_count > 0) {
            size_t data_size = getTallyDataSize(channel_count);
            // little-endian 방식으로 바이트 복사
            uint8_t* dst = (uint8_t*)out_combined_tally;
            for (size_t i = 0; i < data_size && i < 8; i++) {
                dst[i] = data[TALLY_PACKET_HEADER_SIZE + i];
            }
        }
    }

    return ESP_OK;
}

bool isTallyPacket(const uint8_t* data, size_t length)
{
    return getPacketType(data, length) == LoRaPacketType::TALLY;
}

bool isHeartbeatPacket(const uint8_t* data, size_t length)
{
    if (!isTallyPacket(data, length)) {
        return false;
    }

    if (length < 2) {
        return false;
    }

    // channel_count가 0이면 Heartbeat
    return data[1] == 0;
}

size_t createConfigChangePacket(float frequency, uint8_t sync_word,
                                uint8_t* out_buffer, size_t buffer_size)
{
    if (!out_buffer || buffer_size < CONFIG_CHANGE_PACKET_SIZE) {
        return 0;
    }

    ConfigChangePacket packet;
    packet.header = static_cast<uint8_t>(LoRaPacketType::CONFIG_CHANGE);
    packet.frequency = frequency;
    packet.sync_word = sync_word;

    memcpy(out_buffer, &packet, CONFIG_CHANGE_PACKET_SIZE);
    return CONFIG_CHANGE_PACKET_SIZE;
}

esp_err_t parseConfigChangePacket(const uint8_t* data, size_t length,
                                  float* out_frequency, uint8_t* out_sync_word)
{
    if (!data || length < CONFIG_CHANGE_PACKET_SIZE) {
        return ESP_FAIL;
    }

    if (data[0] != static_cast<uint8_t>(LoRaPacketType::CONFIG_CHANGE)) {
        return ESP_FAIL;
    }

    // 패킷 파싱
    const ConfigChangePacket* packet = reinterpret_cast<const ConfigChangePacket*>(data);

    if (out_frequency) {
        *out_frequency = packet->frequency;
    }

    if (out_sync_word) {
        *out_sync_word = packet->sync_word;
    }

    return ESP_OK;
}

bool isConfigChangePacket(const uint8_t* data, size_t length)
{
    return getPacketType(data, length) == LoRaPacketType::CONFIG_CHANGE;
}

// 새로운 F1-F4 패킷 처리 함수들

bool isNewTallyPacket(const uint8_t* data, size_t length)
{
    if (!data || length < 1) {
        return false;
    }

    uint8_t header = data[0];
    return (header == TALLY_PACKET_8CH_HEADER ||
            header == TALLY_PACKET_12CH_HEADER ||
            header == TALLY_PACKET_16CH_HEADER ||
            header == TALLY_PACKET_20CH_HEADER);
}

uint8_t getChannelCountFromHeader(uint8_t header)
{
    switch (header) {
        case TALLY_PACKET_8CH_HEADER:
            return 8;
        case TALLY_PACKET_12CH_HEADER:
            return 12;
        case TALLY_PACKET_16CH_HEADER:
            return 16;
        case TALLY_PACKET_20CH_HEADER:
            return 20;
        default:
            return 0;  // 잘못된 헤더
    }
}

uint8_t getDataLengthFromHeader(uint8_t header)
{
    switch (header) {
        case TALLY_PACKET_8CH_HEADER:
            return 2;   // 8채널 = 2바이트
        case TALLY_PACKET_12CH_HEADER:
            return 3;   // 12채널 = 3바이트
        case TALLY_PACKET_16CH_HEADER:
            return 4;   // 16채널 = 4바이트
        case TALLY_PACKET_20CH_HEADER:
            return 5;   // 20채널 = 5바이트
        default:
            return 0;   // 잘못된 헤더
    }
}

esp_err_t parseNewTallyPacket(const uint8_t* data, size_t length,
                             uint64_t* out_combined_tally,
                             uint8_t* out_channel_count)
{
    if (!data || length < 1) {
        return ESP_FAIL;
    }

    uint8_t header = data[0];
    uint8_t expected_data_len = getDataLengthFromHeader(header);

    if (expected_data_len == 0 || length < (1 + expected_data_len)) {
        return ESP_FAIL;  // 잘못된 헤더나 길이
    }

    // Tally 데이터 재조합 (Little Endian)
    uint64_t combined_tally = 0;
    for (uint8_t i = 0; i < expected_data_len; i++) {
        combined_tally |= ((uint64_t)data[1 + i]) << (i * 8);
    }

    if (out_combined_tally) {
        *out_combined_tally = combined_tally;
    }

    if (out_channel_count) {
        *out_channel_count = getChannelCountFromHeader(header);
    }

    return ESP_OK;
}

} // namespace LoRaPacket
