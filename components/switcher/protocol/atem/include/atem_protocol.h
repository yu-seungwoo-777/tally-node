/**
 * ATEM 프로토콜 정의
 *
 * ATEM UDP 프로토콜의 상수, 플래그, 명령 정의
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef ATEM_PROTOCOL_H
#define ATEM_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "switcher_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 패킷 헤더 플래그
 * ============================================================================ */

#define ATEM_FLAG_ACK_REQUEST      0x01  /* 수신 확인 요청 */
#define ATEM_FLAG_HELLO            0x02  /* Hello 패킷 */
#define ATEM_FLAG_RESEND           0x04  /* 재전송 패킷 */
#define ATEM_FLAG_REQUEST_RESEND   0x08  /* 재전송 요청 */
#define ATEM_FLAG_ACK              0x10  /* 수신 확인 */

/* ============================================================================
 * 프로토콜 상수
 * ============================================================================ */

/* 프로토콜 버전 */
#define ATEM_PROTOCOL_VERSION_MAJOR  2
#define ATEM_PROTOCOL_VERSION_MINOR  29

/* 네트워크 설정 */
#define ATEM_DEFAULT_PORT            9910

/* 패킷 크기 */
#define ATEM_HEADER_LENGTH           12     /* 패킷 헤더 크기 */
#define ATEM_CMD_HEADER_LENGTH       8      /* 명령 헤더 크기 */
#define ATEM_MAX_PACKET_SIZE         1500   /* 최대 패킷 크기 (MTU) */

/* 버퍼 크기 */
#define ATEM_RX_BUFFER_SIZE          1500   /* 수신 버퍼 (MTU 전체) */
#define ATEM_TX_BUFFER_SIZE          64     /* 송신 버퍼 (명령 패킷) */

/* 초기화 설정 */
#define ATEM_MAX_INIT_PACKETS        500    /* 최대 초기화 패킷 수 */
#define ATEM_INIT_TIMEOUT_MS         10000  /* 초기화 타임아웃 (ms) */

/* 타임아웃 설정 (switcher_config.h에서 공통 관리) */
#define ATEM_CONNECT_TIMEOUT_MS      SWITCHER_CONNECT_TIMEOUT_MS
#define ATEM_RESPONSE_TIMEOUT_MS     SWITCHER_RESPONSE_TIMEOUT_MS
#define ATEM_MAX_SILENCE_TIME_MS     SWITCHER_MAX_SILENCE_TIME_MS
/* ATEM_KEEPALIVE_INTERVAL_MS는 switcher_config.h에서 직접 정의 */

/* 재시도 설정 (switcher_config.h에서 공통 관리) */
#define ATEM_MAX_RETRIES             SWITCHER_MAX_RETRIES
#define ATEM_RETRY_DELAY_MS          SWITCHER_RETRY_DELAY_MS

/* 상태 저장 제한 (메모리 최적화) */
#define ATEM_MAX_MES                 8      /* 최대 Mix Effect 수 */
#define ATEM_MAX_CHANNELS            20     /* Tally 패킹 최대 채널 수 (SWITCHER_MAX_CHANNELS와 동일) */
#define ATEM_MAX_SOURCES             20     /* 최대 소스 수 (카메라 입력, ATEM_MAX_CHANNELS와 동일) */
#define ATEM_MAX_KEYERS              4      /* ME당 최대 Keyer 수 */
#define ATEM_MAX_DSKS                4      /* 최대 DSK 수 */
#define ATEM_PRODUCT_NAME_LEN        64     /* 제품명 최대 길이 */
#define ATEM_INPUT_LONG_NAME_LEN     21     /* Input Long Name 길이 (20 + null) */
#define ATEM_INPUT_SHORT_NAME_LEN    5      /* Input Short Name 길이 (4 + null) */
#define ATEM_MAX_INPUTS              64     /* 최대 입력 소스 수 (저장용) */

/* ============================================================================
 * Tally 상태 값
 *
 * ATEM 프로토콜과 동일한 비트 배치:
 *   - bit0 (0x01) = Program
 *   - bit1 (0x02) = Preview
 *
 * packed 형식 (uint64_t):
 *   - 20채널 × 2비트 = 40비트 사용
 *   - CH1은 bit[1:0], CH2는 bit[3:2], ... CH20은 bit[39:38]
 * ============================================================================ */

#define ATEM_TALLY_OFF               0      /* 0b00: Off */
#define ATEM_TALLY_PROGRAM           1      /* 0b01: Program (bit0) */
#define ATEM_TALLY_PREVIEW           2      /* 0b10: Preview (bit1) */
#define ATEM_TALLY_BOTH              3      /* 0b11: Program + Preview */

/**
 * 비트 플래그를 Tally 상태 값으로 변환
 *
 * ATEM 프로토콜 플래그를 그대로 사용 (bit0=PGM, bit1=PVW)
 *
 * @param flags ATEM 프로토콜 플래그 (TlIn 명령의 각 채널 바이트)
 * @return Tally 상태 값 (0~3)
 */
static inline uint8_t atem_tally_from_flags(uint8_t flags)
{
    return flags & 0x03;
}

/**
 * 패킹된 Tally에서 채널 상태 조회
 *
 * @param packed 패킹된 Tally (64비트)
 * @param index 채널 인덱스 (0=CAM1, 1=CAM2, ... 19=CAM20)
 * @return Tally 상태 값 (ATEM_TALLY_OFF/PROGRAM/PREVIEW/BOTH)
 */
static inline uint8_t atem_tally_get(uint64_t packed, uint8_t index)
{
    if (index >= ATEM_MAX_CHANNELS) return 0;
    return (packed >> (index * 2)) & 0x03;
}

/**
 * 패킹된 Tally에 채널 상태 설정
 *
 * @param packed 패킹된 Tally 포인터
 * @param index 채널 인덱스 (0=CAM1, 1=CAM2, ... 19=CAM20)
 * @param value Tally 상태 값 (ATEM_TALLY_OFF/PROGRAM/PREVIEW/BOTH)
 */
static inline void atem_tally_set(uint64_t* packed, uint8_t index, uint8_t value)
{
    if (index >= ATEM_MAX_CHANNELS) return;
    uint8_t shift = index * 2;
    *packed &= ~((uint64_t)0x03 << shift);
    *packed |= ((uint64_t)(value & 0x03) << shift);
}

/* ============================================================================
 * 소스 ID (Source ID)
 *
 * Program/Preview 입력 변경 시 사용
 * atem_client_set_program_input(client, me, ATEM_SOURCE_BLACK);
 * ============================================================================ */

/* 특수 소스 */
#define ATEM_SOURCE_BLACK            0       /* Black */
#define ATEM_SOURCE_BARS             1000    /* Color Bars */
#define ATEM_SOURCE_COLOR1           2001    /* Color Generator 1 */
#define ATEM_SOURCE_COLOR2           2002    /* Color Generator 2 */
#define ATEM_SOURCE_MEDIA_PLAYER1    3010    /* Media Player 1 */
#define ATEM_SOURCE_MEDIA_PLAYER1_KEY 3011   /* Media Player 1 Key */
#define ATEM_SOURCE_MEDIA_PLAYER2    3020    /* Media Player 2 */
#define ATEM_SOURCE_MEDIA_PLAYER2_KEY 3021   /* Media Player 2 Key */
#define ATEM_SOURCE_SUPERSOURCE      6000    /* SuperSource */
#define ATEM_SOURCE_CLEAN_FEED1      7001    /* Clean Feed 1 */
#define ATEM_SOURCE_CLEAN_FEED2      7002    /* Clean Feed 2 */
#define ATEM_SOURCE_AUX1             8001    /* Aux 1 */
#define ATEM_SOURCE_AUX2             8002    /* Aux 2 */
#define ATEM_SOURCE_AUX3             8003    /* Aux 3 */
#define ATEM_SOURCE_AUX4             8004    /* Aux 4 */
#define ATEM_SOURCE_AUX5             8005    /* Aux 5 */
#define ATEM_SOURCE_AUX6             8006    /* Aux 6 */
#define ATEM_SOURCE_PROGRAM          10010   /* Program */
#define ATEM_SOURCE_PREVIEW          10011   /* Preview */

/* 카메라 입력 (1-20) */
#define ATEM_SOURCE_CAM(n)           (n)     /* Camera 1-20: ATEM_SOURCE_CAM(1) = 1 */

/* ============================================================================
 * 명령 문자열 (4글자 고정)
 * ============================================================================ */

/* 기본 정보 */
#define ATEM_CMD_VERSION             "_ver"  /* Protocol Version */
#define ATEM_CMD_PRODUCT_ID          "_pin"  /* Product ID */
#define ATEM_CMD_TOPOLOGY            "_top"  /* Topology */
#define ATEM_CMD_ME_CONFIG           "_MeC"  /* Mix Effect Config */
#define ATEM_CMD_TALLY_CONFIG        "_TlC"  /* Tally Channel Config */
#define ATEM_CMD_INPUT_PROP          "InPr"  /* Input Properties */

/* 실시간 상태 */
#define ATEM_CMD_PROGRAM_INPUT       "PrgI"  /* Program Input */
#define ATEM_CMD_PREVIEW_INPUT       "PrvI"  /* Preview Input */
#define ATEM_CMD_TALLY_INDEX         "TlIn"  /* Tally By Index */
#define ATEM_CMD_TALLY_SOURCE        "TlSr"  /* Tally By Source */

/* Transition */
#define ATEM_CMD_TRANSITION_SETTINGS "TrSS"  /* Transition Settings */
#define ATEM_CMD_TRANSITION_POSITION "TrPs"  /* Transition Position */
#define ATEM_CMD_TRANSITION_PREVIEW  "TrPr"  /* Transition Preview */

/* Keyer */
#define ATEM_CMD_KEYER_ON_AIR        "KeOn"  /* Keyer On Air */
#define ATEM_CMD_DSK_STATE           "DskS"  /* Downstream Keyer State */
#define ATEM_CMD_DSK_PROPERTIES      "DskP"  /* Downstream Keyer Properties (Tie 포함) */
#define ATEM_CMD_SUPERSOURCE         "SSrc"  /* SuperSource */

/* 제어 명령 (클라이언트 → 스위처) */
#define ATEM_CMD_CUT                 "DCut"  /* Cut */
#define ATEM_CMD_AUTO                "DAut"  /* Auto */
#define ATEM_CMD_CHANGE_PROGRAM      "CPgI"  /* Change Program Input */
#define ATEM_CMD_CHANGE_PREVIEW      "CPvI"  /* Change Preview Input */

/* DSK 제어 명령 */
#define ATEM_CMD_DSK_ON_AIR          "CDsL"  /* DSK On Air (Set) */
#define ATEM_CMD_DSK_AUTO            "DDsA"  /* DSK Auto */
#define ATEM_CMD_DSK_TIE             "CDsT"  /* DSK Tie (Set) */

/* USK 제어 명령 */
#define ATEM_CMD_USK_ON_AIR          "CKOn"  /* USK On Air (Set) */

/* Transition Next 제어 */
#define ATEM_CMD_TRANSITION_NEXT     "CTTp"  /* Change Transition Type (Next BKGD/Key) */

/* 초기화 완료 */
#define ATEM_CMD_INIT_COMPLETE       "InCm"  /* Initialization Complete */

/* ============================================================================
 * 유틸리티 함수
 * ============================================================================ */

/**
 * 명령 문자열 비교 (4글자)
 *
 * @param cmd1 명령 문자열 1
 * @param cmd2 명령 문자열 2
 * @return 같으면 true
 */
static inline bool atem_cmd_equals(const char* cmd1, const char* cmd2)
{
    return (cmd1[0] == cmd2[0] &&
            cmd1[1] == cmd2[1] &&
            cmd1[2] == cmd2[2] &&
            cmd1[3] == cmd2[3]);
}

/**
 * 명령 문자열 복사 (4글자 + null)
 *
 * @param dest 목적지 (최소 5바이트)
 * @param src 소스
 */
static inline void atem_cmd_copy(char* dest, const char* src)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
    dest[4] = '\0';
}

#ifdef __cplusplus
}
#endif

#endif /* ATEM_PROTOCOL_H */
