/**
 * @file TallyTypes.cpp
 * @brief Tally 공통 타입 구현
 */

#include "TallyTypes.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// PackedData 함수 구현 (정적 버퍼)
// ============================================================================

void packed_data_init(packed_data_t* packed, uint8_t channel_count)
{
    if (!packed) return;

    packed->channel_count = (channel_count > TALLY_MAX_CHANNELS) ? TALLY_MAX_CHANNELS : channel_count;

    if (channel_count > 0) {
        // (채널 수 + 3) / 4 바이트 계산
        uint8_t byte_count = (channel_count + 3) / 4;
        // 최대 버퍼 크기 제한 (8바이트)
        if (byte_count > sizeof(packed->data)) {
            byte_count = sizeof(packed->data);
        }
        packed->data_size = byte_count;
        // 버퍼 초기화
        for (uint8_t i = 0; i < byte_count; i++) {
            packed->data[i] = 0;
        }
    } else {
        packed->data_size = 0;
    }
}

void packed_data_cleanup(packed_data_t* packed)
{
    if (!packed) return;

    // 정적 버퍼이므로 해제 불필요, 단지 초기화만
    packed->data_size = 0;
    packed->channel_count = 0;
}

void packed_data_set_channel(packed_data_t* packed, uint8_t channel, uint8_t flags)
{
    if (!packed || !packed->data) return;
    if (channel < 1 || channel > packed->channel_count) return;

    uint8_t byte_index = (channel - 1) / 4;
    uint8_t bit_offset = ((channel - 1) % 4) * 2;

    if (byte_index < packed->data_size) {
        packed->data[byte_index] &= ~(0x03 << bit_offset);      // 클리어
        packed->data[byte_index] |= ((flags & 0x03) << bit_offset); // 설정
    }
}

uint8_t packed_data_get_channel(const packed_data_t* packed, uint8_t channel)
{
    if (!packed || !packed->data) return 0;
    if (channel < 1 || channel > packed->channel_count) return 0;

    uint8_t byte_index = (channel - 1) / 4;
    uint8_t bit_offset = ((channel - 1) % 4) * 2;

    if (byte_index < packed->data_size) {
        return (packed->data[byte_index] >> bit_offset) & 0x03;
    }

    return 0;
}

void packed_data_copy(packed_data_t* dest, const packed_data_t* src)
{
    if (!dest || !src) return;

    // 기존 데이터 해제
    packed_data_cleanup(dest);

    // 새로 할당
    dest->channel_count = src->channel_count;
    dest->data_size = src->data_size;

    if (src->data_size > 0) {
        dest->data = (uint8_t*) malloc(src->data_size);
        if (dest->data) {
            memcpy(dest->data, src->data, src->data_size);
        }
    } else {
        dest->data = NULL;
    }
}

bool packed_data_equals(const packed_data_t* a, const packed_data_t* b)
{
    if (!a || !b) return false;

    if (a->channel_count != b->channel_count) return false;
    if (a->data_size != b->data_size) return false;

    if (a->data_size > 0) {
        if (!a->data || !b->data) return false;
        return memcmp(a->data, b->data, a->data_size) == 0;
    }

    return true;
}

bool packed_data_is_valid(const packed_data_t* packed)
{
    if (!packed) return false;

    if (packed->channel_count == 0) return false;
    if (packed->data_size == 0) return false;
    if (!packed->data) return false;

    // 데이터 크기 검증: (채널 수 + 3) / 4
    uint8_t expected_size = (packed->channel_count + 3) / 4;
    return packed->data_size == expected_size;
}

uint64_t packed_data_to_uint64(const packed_data_t* packed)
{
    if (!packed || !packed->data) return 0;

    uint64_t result = 0;
    uint8_t max_bytes = (packed->data_size < 8) ? packed->data_size : 8;

    for (uint8_t i = 0; i < max_bytes; i++) {
        result |= (static_cast<uint64_t>(packed->data[i]) << (i * 8));
    }

    return result;
}

void packed_data_from_uint64(packed_data_t* packed, uint64_t value, uint8_t channel_count)
{
    if (!packed) return;

    // 기존 데이터 해제
    packed_data_cleanup(packed);

    packed->channel_count = channel_count;
    uint8_t byte_count = (channel_count + 3) / 4;
    packed->data_size = byte_count;

    if (byte_count > 0) {
        packed->data = (uint8_t*) malloc(byte_count);
        if (packed->data) {
            for (uint8_t i = 0; i < byte_count; i++) {
                packed->data[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
            }
        }
    }
}

// ============================================================================
// SwitcherConfig 함수 구현
// ============================================================================

void switcher_config_init(switcher_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(switcher_config_t));

    config->name = "Switcher";
    config->type = SWITCHER_TYPE_ATEM;
    config->interface = TALLY_NET_ETHERNET;
    config->ip[0] = '\0';
    config->port = 0;
    config->password[0] = '\0';
    config->camera_limit = 0;
}

// ============================================================================
// SwitcherStatus 함수 구현
// ============================================================================

void switcher_status_init(switcher_status_t* status)
{
    if (!status) return;

    memset(status, 0, sizeof(switcher_status_t));
    status->state = CONNECTION_STATE_DISCONNECTED;
    status->camera_count = 0;
    status->last_update_time = 0;
    status->tally_changed = false;
}

// ============================================================================
// 유틸리티 함수 구현
// ============================================================================

const char* switcher_type_to_string(switcher_type_t type)
{
    switch (type) {
        case SWITCHER_TYPE_ATEM:
            return "ATEM";
        case SWITCHER_TYPE_OBS:
            return "OBS";
        case SWITCHER_TYPE_VMIX:
            return "VMIX";
        default:
            return "UNKNOWN";
    }
}

const char* connection_state_to_string(connection_state_t state)
{
    switch (state) {
        case CONNECTION_STATE_DISCONNECTED:
            return "DISCONNECTED";
        case CONNECTION_STATE_CONNECTING:
            return "CONNECTING";
        case CONNECTION_STATE_CONNECTED:
            return "CONNECTED";
        case CONNECTION_STATE_INITIALIZING:
            return "INITIALIZING";
        case CONNECTION_STATE_READY:
            return "READY";
        default:
            return "UNKNOWN";
    }
}

const char* tally_status_to_string(tally_status_t status)
{
    switch (status) {
        case TALLY_STATUS_OFF:
            return "OFF";
        case TALLY_STATUS_PROGRAM:
            return "PROGRAM";
        case TALLY_STATUS_PREVIEW:
            return "PREVIEW";
        case TALLY_STATUS_BOTH:
            return "BOTH";
        default:
            return "UNKNOWN";
    }
}
