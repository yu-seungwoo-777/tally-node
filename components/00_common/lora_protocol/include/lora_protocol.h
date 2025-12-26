/**
 * @file lora_protocol.h
 * @brief LoRa 패킷 프로토콜 정의
 *
 * Tally 데이터(TX→RX), 관리 명령(TX→RX), 상태 응답(RX→TX)
 */

#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 헤더 정의
// ============================================================================

// Tally 데이터 (기존)
#define LORA_HDR_TALLY_8CH     0xF1   // 8채널
#define LORA_HDR_TALLY_12CH    0xF2   // 12채널
#define LORA_HDR_TALLY_16CH    0xF3   // 16채널
#define LORA_HDR_TALLY_20CH    0xF4   // 20채널

// TX → RX 명령
#define LORA_HDR_STATUS_REQ    0xE0   // 상태 요청 (Broadcast)
#define LORA_HDR_SET_BRIGHTNESS 0xE1  // 밝기 설정 (Unicast)
#define LORA_HDR_SET_CAMERA_ID 0xE2   // 카메라 ID 설정 (Unicast)
#define LORA_HDR_SET_RF        0xE3   // 주파수+SyncWord 설정 (Unicast)
#define LORA_HDR_STOP          0xE4   // 기능 정지 (Uni/Broadcast)
#define LORA_HDR_REBOOT        0xE5   // 재부팅 (Unicast)
#define LORA_HDR_PING          0xE6   // 지연시간 테스트 (Unicast)

// RX → TX 응답
#define LORA_HDR_STATUS        0xD0   // 상태 정보
#define LORA_HDR_ACK           0xD1   // 명령 승인
#define LORA_HDR_PONG          0xD2   // PING 응답

// Device ID 길이 (MAC 뒤 4자리)
#define LORA_DEVICE_ID_LEN     4
#define LORA_BROADCAST_ID      {0xFF, 0xFF, 0xFF, 0xFF}

// ============================================================================
// 패킷 구조체
// ============================================================================

/**
 * @brief 밝기 설정 명령 (0xE1)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE1
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint8_t brightness;              // 0-100
} lora_cmd_brightness_t;

/**
 * @brief 카메라 ID 설정 명령 (0xE2)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE2
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint8_t camera_id;
} lora_cmd_camera_id_t;

/**
 * @brief RF 설정 명령 (0xE3)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE3
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    float frequency;                 // MHz
    uint8_t sync_word;
} lora_cmd_rf_t;

/**
 * @brief 기능 정지 명령 (0xE4)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE4
    uint8_t device_id[LORA_DEVICE_ID_LEN];
} lora_cmd_stop_t;

/**
 * @brief 재부팅 명령 (0xE5)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE5
    uint8_t device_id[LORA_DEVICE_ID_LEN];
} lora_cmd_reboot_t;

/**
 * @brief PING 명령 (0xE6)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xE6
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint32_t timestamp;              // 송신 시간 (ms)
} lora_cmd_ping_t;

/**
 * @brief 상태 응답 (0xD0)
 * @note RSSI/SNR은 수신 시 패킷에서 직접 취득
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xD0
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint8_t battery;                 // 0-100
    uint8_t camera_id;
    uint32_t uptime;                 // 초
    uint8_t brightness;              // 0-100
    uint8_t flags;                   // 비트 플래그
} lora_msg_status_t;

// Status 플래그
#define LORA_STATUS_FLAG_STOPPED   0x01  // 기능 정지 상태

/**
 * @brief ACK 응답 (0xD1)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xD1
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint8_t cmd_header;              // 승인할 명령 헤더
    uint8_t result;                  // 0: 성공, 그외: 에러 코드
} lora_msg_ack_t;

// ACK 결과 코드
#define LORA_ACK_SUCCESS       0x00
#define LORA_ACK_ERR_UNKNOWN   0x01
#define LORA_ACK_ERR_INVALID   0x02
#define LORA_ACK_ERR_FAILED    0x03

/**
 * @brief PONG 응답 (0xD2)
 */
typedef struct __attribute__((packed)) {
    uint8_t header;                  // 0xD2
    uint8_t device_id[LORA_DEVICE_ID_LEN];
    uint32_t tx_timestamp;           // PING의 timestamp
    uint32_t rx_timestamp;           // 수신 시간 (ms)
} lora_msg_pong_t;

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief Device ID 비교
 */
static inline bool lora_device_id_equals(const uint8_t* id1, const uint8_t* id2) {
    return (id1[0] == id2[0] && id1[1] == id2[1] &&
            id1[2] == id2[2] && id1[3] == id2[3]);
}

/**
 * @brief Broadcast ID 확인
 */
static inline bool lora_device_id_is_broadcast(const uint8_t* id) {
    return (id[0] == 0xFF && id[1] == 0xFF &&
            id[2] == 0xFF && id[3] == 0xFF);
}

/**
 * @brief Device ID 문자열 변환 (5바이트 버퍼 필요)
 */
static inline void lora_device_id_to_str(const uint8_t* id, char* str) {
    str[0] = id[0];
    str[1] = id[1];
    str[2] = id[2];
    str[3] = id[3];
    str[4] = '\0';
}

/**
 * @brief 헤더가 TX→RX 명령인지 확인
 */
static inline bool lora_header_is_tx_command(uint8_t header) {
    return (header >= 0xE0 && header <= 0xEF);
}

/**
 * @brief 헤더가 RX→TX 응답인지 확인
 */
static inline bool lora_header_is_rx_response(uint8_t header) {
    return (header >= 0xD0 && header <= 0xDF);
}

/**
 * @brief 헤더가 Tally 데이터인지 확인
 */
static inline bool lora_header_is_tally(uint8_t header) {
    return (header >= 0xF1 && header <= 0xF4);
}

#ifdef __cplusplus
}
#endif

#endif // LORA_PROTOCOL_H
