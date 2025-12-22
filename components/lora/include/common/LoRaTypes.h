/**
 * @file LoRaTypes.h
 * @brief LoRa 컴포넌트 공용 타입 정의
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
#include <functional>
#include <atomic>
#include <queue>
#include <map>
#include <vector>
#endif

// LoRa 칩 타입
typedef enum {
    LORA_CHIP_SX1262 = 0,
    LORA_CHIP_SX1268,
    LORA_CHIP_UNKNOWN
} lora_chip_type_t;

// LoRa 상태 구조체
typedef struct {
    bool is_initialized;
    lora_chip_type_t chip_type;
    float frequency;
    float freq_min;
    float freq_max;
    uint8_t sync_word;
    int16_t rssi;
    int16_t snr;
} lora_status_t;

// 채널 정보
typedef struct {
    float frequency;
    int16_t rssi;
    float noise_floor;
    bool clear_channel;
} channel_info_t;

// 패킷 타입
typedef enum {
    LORA_PACKET_TALLY = 0xAA,
    LORA_PACKET_STATUS = 0xBB,
    LORA_PACKET_CONFIG_CHANGE = 0xFC,
    LORA_PACKET_PING = 0xCC,
    LORA_PACKET_PONG = 0xDD,
    LORA_PACKET_UNKNOWN = 0xFF
} lora_packet_type_t;

// Tally 패킷 헤더
typedef struct {
    uint8_t header;
    uint8_t channel_count;
    uint8_t sequence;
} __attribute__((packed)) tally_packet_header_t;

// 최대 Tally 패킷 구조
typedef struct {
    tally_packet_header_t header;
    uint64_t combined_tally;
} __attribute__((packed)) tally_packet_t;

// 상태 패킷 구조
typedef struct {
    uint8_t header;
    uint8_t device_id;
    uint8_t battery_level;
    int8_t rssi;
    uint32_t uptime;
} __attribute__((packed)) status_packet_t;

// 설정 변경 패킷 구조
typedef struct {
    uint8_t header;
    float frequency;
    uint8_t sync_word;
    uint8_t power;
} __attribute__((packed)) config_change_packet_t;

// 실시간 성능 요구사항
#define LORA_MAX_LATENCY_US     10000    // 최대 지연 10ms
#define LORA_UPDATE_RATE_HZ     60       // 60Hz 업데이트
#define LORA_MAX_JITTER_US      1000     // 최대 지터 1ms
#define LORA_WATCHDOG_TIMEOUT_MS 50     // 워치독 50ms

// 패킷 크기 상수
#define LORA_TALLY_PACKET_MAX_SIZE     32
#define LORA_STATUS_PACKET_SIZE        sizeof(status_packet_t)
#define LORA_CONFIG_PACKET_SIZE        sizeof(config_change_packet_t)
#define LORA_MAX_PACKET_SIZE           256

// 채널 설정
#define LORA_MAX_CHANNELS              32
#define LORA_MAX_SWITCHERS             2
#define LORA_SEQUENCE_MAX              65535

#ifdef __cplusplus

// C++ 타입 정의
class LoRaManager;
class RealtimeTallyDispatcher;
class FastTallyMapper;
class PacketRouter;
class DeviceManager;

// 콜백 타입 (LoRaCore.h의 C 콜백과 호환)
#ifdef __cplusplus
typedef std::function<void(const uint8_t*, size_t)> LoRaReceiveCallbackCpp;
#endif
typedef void (*LoRaReceiveCallback)(const uint8_t* data, size_t length);
typedef std::function<void(uint32_t)> PacketTransmitCallback;

// Tally 이벤트
struct TallyEvent {
    uint64_t timestamp;
    uint8_t switcher_id;
    uint64_t tally_data;
    uint8_t channel_count;
    bool is_primary;
};

// 장치 정보
struct DeviceInfo {
    uint8_t device_id;
    uint8_t camera_id;
    bool is_online;
    uint64_t last_seen;
    lora_status_t status;
};

// 성능 메트릭
struct PerformanceMetrics {
    uint64_t min_latency_us;
    uint64_t max_latency_us;
    uint64_t avg_latency_us;
    uint32_t missed_deadlines;
    float cpu_usage_percent;
    uint32_t packets_per_second;
};

// 맵핑 테이블
struct MappingTable {
    uint8_t channel_to_switcher[LORA_MAX_CHANNELS];
    uint8_t channel_to_index[LORA_MAX_CHANNELS];
    uint8_t offsets[LORA_MAX_SWITCHERS];
    uint8_t limits[LORA_MAX_SWITCHERS];
    uint8_t active_switchers;
};

// 스위처 설정
struct SwitcherConfig {
    uint8_t switcher_id;
    uint8_t offset;
    uint8_t camera_count;
    bool enabled;
    uint64_t last_tally;
};

#endif // __cplusplus