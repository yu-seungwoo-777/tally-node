/**
 * ATEM 디버그 시스템
 *
 * Simple Log 시스템 사용
 */

#ifndef ATEM_DEBUG_H
#define ATEM_DEBUG_H

#include "log.h"
#include "log_tags.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 패킷 디버그 활성화 (기본값: 0) */
#ifndef ATEM_DEBUG_PACKET
    #define ATEM_DEBUG_PACKET 0
#endif

/* ============================================================================
 * 디버그 매크로 (Simple Log 사용)
 * ============================================================================ */

#define ATEM_LOGE(fmt, ...) LOG_0(TAG_ATEM, fmt, ##__VA_ARGS__)
#define ATEM_LOGW(fmt, ...) LOG_0(TAG_ATEM, fmt, ##__VA_ARGS__)
#define ATEM_LOGI(fmt, ...) LOG_0(TAG_ATEM, fmt, ##__VA_ARGS__)
#define ATEM_LOGV(fmt, ...) LOG_1(TAG_ATEM, fmt, ##__VA_ARGS__)

/* 패킷 덤프 매크로 */
#if ATEM_DEBUG_PACKET
    #define ATEM_DUMP_TX(data, len) atem_debug_dump_packet("TX", data, len)
    #define ATEM_DUMP_RX(data, len) atem_debug_dump_packet("RX", data, len)
    #define ATEM_DUMP_CMD(cmd, data, len) atem_debug_dump_command(cmd, data, len)
    #define ATEM_DUMP_HEADER(data) atem_debug_dump_header(data)
#else
    #define ATEM_DUMP_TX(data, len) ((void)0)
    #define ATEM_DUMP_RX(data, len) ((void)0)
    #define ATEM_DUMP_CMD(cmd, data, len) ((void)0)
    #define ATEM_DUMP_HEADER(data) ((void)0)
#endif

/* ============================================================================
 * ATEM 전용 패킷 덤프 함수
 * ============================================================================ */

/**
 * 패킷 원본 덤프 (16진수)
 *
 * 출력 형식:
 *   [TX] 24 bytes:
 *   0000: 10 14 00 00 00 00 00 00 00 3a 00 00 01 00 00 00
 *   0010: 00 00 00 00 00 00 00 00
 *
 * @param direction "TX" 또는 "RX"
 * @param data 패킷 데이터
 * @param length 패킷 길이
 */
void atem_debug_dump_packet(const char* direction, const uint8_t* data, uint16_t length);

/**
 * 명령 데이터 덤프
 *
 * 출력 형식:
 *   [CMD] PrgI (4 bytes): 00 00 00 01
 *
 * @param cmd_name 명령 이름 (4글자)
 * @param data 명령 데이터
 * @param length 데이터 길이
 */
void atem_debug_dump_command(const char* cmd_name, const uint8_t* data, uint16_t length);

/**
 * 패킷 헤더 파싱 및 출력
 *
 * 출력 형식:
 *   [HDR] flags=0x01 len=24 session=0x1234 ack=0 pkt=5
 *
 * @param data 패킷 데이터 (최소 12바이트)
 */
void atem_debug_dump_header(const uint8_t* data);

/* ============================================================================
 * 유틸리티
 * ============================================================================ */

/**
 * 플래그 문자열 변환
 *
 * @param flags 패킷 플래그
 * @param buf 출력 버퍼 (최소 32바이트)
 * @return buf 포인터
 */
const char* atem_debug_flags_str(uint8_t flags, char* buf);

/* ============================================================================
 * 상태 출력
 * ============================================================================ */

/* Forward declaration */
struct atem_client_t;

/**
 * 토폴로지 정보 출력
 *
 * 출력 형식:
 *   ┌────────────────────────────────────────┐
 *   │ ATEM Topology                          │
 *   ├────────────────────────────────────────┤
 *   │ Product    : ATEM Mini                 │
 *   │ Protocol   : 2.30                      │
 *   │ ME         : 1                         │
 *   │ Sources    : 14                        │
 *   │ Cameras    : 4                         │
 *   │ DSK        : 1                         │
 *   │ USK (ME0)  : 1                         │
 *   │ SuperSrc   : 없음                       │
 *   └────────────────────────────────────────┘
 *
 * @param client 클라이언트 구조체 포인터
 */
void atem_debug_print_topology(const void* client);

#ifdef __cplusplus
}
#endif

#endif /* ATEM_DEBUG_H */
