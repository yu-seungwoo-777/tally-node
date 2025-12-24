/**
 * @file AtemProtocol.h
 * @brief ATEM UDP 프로토콜 상수 및 유틸리티
 *
 * 역할: ATEM 프로토콜 정의
 * - 패킷 플래그, 명령 문자열, 바이트 순서 유틸리티
 */

#ifndef ATEM_PROTOCOL_H
#define ATEM_PROTOCOL_H

#include <stdint.h>
#include <cstring>

// ============================================================================
// 프로토콜 상수
// ============================================================================

#define ATEM_DEFAULT_PORT           9910        ///< 기본 포트
#define ATEM_HEADER_LENGTH          12          ///< 패킷 헤더 길이 (정확히 12바이트)
#define ATEM_CMD_HEADER_LENGTH      8           ///< 명령 헤더 길이 (정확히 8바이트)
#define ATEM_HELLO_PACKET_SIZE      20          ///< Hello 패킷 크기 (정확히 20바이트)
#define ATEM_ACK_PACKET_SIZE        12          ///< ACK 패킷 크기 (정확히 12바이트)
#define ATEM_MAX_PACKET_SIZE        1500        ///< 최대 패킷 크기 (MTU)

// 타임아웃 설정
#define ATEM_CONNECT_TIMEOUT_MS     5000        ///< 연결 타임아웃 (5초)
#define ATEM_MAX_SILENCE_TIME_MS    5000        ///< 최대 무응답 시간 (5초)
#define ATEM_KEEPALIVE_INTERVAL_MS  1000        ///< Keepalive 간격 (1초)

// ============================================================================
// 패킷 헤더 플래그 (5비트)
// ============================================================================

#define ATEM_FLAG_ACK_REQUEST       0x01        ///< 수신 확인 요청
#define ATEM_FLAG_HELLO             0x02        ///< Hello 패킷
#define ATEM_FLAG_RESEND            0x04        ///< 재전송 패킷
#define ATEM_FLAG_REQUEST_RESEND    0x08        ///< 재전송 요청
#define ATEM_FLAG_ACK               0x10        ///< 수신 확인

// ============================================================================
// 명령 문자열 (4글자 고정)
// ============================================================================

#define ATEM_CMD_VERSION            "_ver"      ///< Protocol Version
#define ATEM_CMD_PRODUCT_ID         "_pin"      ///< Product ID
#define ATEM_CMD_TOPOLOGY           "_top"      ///< Topology
#define ATEM_CMD_ME_CONFIG          "_MeC"      ///< Mix Effect Config
#define ATEM_CMD_TALLY_CONFIG       "_TlC"      ///< Tally Channel Config
#define ATEM_CMD_PROGRAM_INPUT      "PrgI"      ///< Program Input
#define ATEM_CMD_PREVIEW_INPUT      "PrvI"      ///< Preview Input
#define ATEM_CMD_TALLY_INDEX        "TlIn"      ///< Tally By Index
#define ATEM_CMD_KEYER_ON_AIR       "KeOn"      ///< Keyer On Air
#define ATEM_CMD_DSK_STATE          "DskS"      ///< DSK State
#define ATEM_CMD_DSK_PROPERTIES     "DskP"      ///< DSK Properties
#define ATEM_CMD_INIT_COMPLETE      "InCm"      ///< Initialization Complete
#define ATEM_CMD_CUT                "DCut"      ///< Cut
#define ATEM_CMD_AUTO               "DAut"      ///< Auto
#define ATEM_CMD_CHANGE_PREVIEW     "CPvI"      ///< Change Preview Input

#ifdef __cplusplus

namespace AtemProtocol {

// ============================================================================
// 바이트 순서 유틸리티 (Big-Endian)
// ============================================================================

/**
 * @brief 16비트 읽기 (Big-Endian)
 */
inline uint16_t getU16(const uint8_t* data, uint16_t offset) {
    return (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
}

/**
 * @brief 16비트 쓰기 (Big-Endian)
 */
inline void setU16(uint8_t* data, uint16_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value >> 8);
    data[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

/**
 * @brief 32비트 읽기 (Big-Endian)
 */
inline uint32_t getU32(const uint8_t* data, uint16_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           data[offset + 3];
}

/**
 * @brief 32비트 쓰기 (Big-Endian)
 */
inline void setU32(uint8_t* data, uint16_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value >> 24);
    data[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

/**
 * @brief 명령 문자열 비교 (4글자)
 */
inline bool cmdEquals(const char* cmd1, const char* cmd2) {
    return (cmd1[0] == cmd2[0] &&
            cmd1[1] == cmd2[1] &&
            cmd1[2] == cmd2[2] &&
            cmd1[3] == cmd2[3]);
}

/**
 * @brief 명령 문자열 복사 (4글자 + null)
 */
inline void cmdCopy(char* dest, const char* src) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
    dest[4] = '\0';
}

/**
 * @brief 명령 문자열 쓰기 (4글자)
 */
inline void setCommand(uint8_t* data, uint16_t offset, const char* cmd) {
    data[offset] = static_cast<uint8_t>(cmd[0]);
    data[offset + 1] = static_cast<uint8_t>(cmd[1]);
    data[offset + 2] = static_cast<uint8_t>(cmd[2]);
    data[offset + 3] = static_cast<uint8_t>(cmd[3]);
}

} // namespace AtemProtocol

#endif // __cplusplus

#endif // ATEM_PROTOCOL_H
