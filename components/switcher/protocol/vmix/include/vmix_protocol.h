/**
 * vMix 프로토콜 정의
 *
 * vMix TCP API 프로토콜의 상수, 명령 정의
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef VMIX_PROTOCOL_H
#define VMIX_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "switcher_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 네트워크 설정
 * ============================================================================ */

#define VMIX_DEFAULT_PORT            8099   /* vMix TCP API 고정 포트 */

/* ============================================================================
 * 버퍼 크기
 * ============================================================================ */

#define VMIX_RX_BUFFER_SIZE          2048   /* 수신 버퍼 (Tally 최대 1011바이트) */
#define VMIX_TX_BUFFER_SIZE          256    /* 송신 버퍼 */
#define VMIX_LINE_BUFFER_SIZE        1100   /* 라인 버퍼 (TALLY 응답용) */

/* ============================================================================
 * 타임아웃 설정 (switcher_config.h에서 공통 관리)
 * ============================================================================ */

#define VMIX_CONNECT_TIMEOUT_MS      SWITCHER_CONNECT_TIMEOUT_MS
#define VMIX_RESPONSE_TIMEOUT_MS     SWITCHER_RESPONSE_TIMEOUT_MS
#define VMIX_MAX_SILENCE_TIME_MS     SWITCHER_MAX_SILENCE_TIME_MS
// VMIX_KEEPALIVE_INTERVAL_MS는 switcher_config.h에서 정의 (3초로 설정)
#define VMIX_RECONNECT_INTERVAL_MS   SWITCHER_RECONNECT_INTERVAL_MS

/* ============================================================================
 * 상태 저장 제한
 * ============================================================================ */

#define VMIX_MAX_INPUTS              64     /* 최대 입력 수 (Tally용) */
#define VMIX_MAX_CHANNELS            20     /* Tally 패킹 최대 채널 수 (SWITCHER_MAX_CHANNELS와 동일) */
#define VMIX_MAX_TALLY_INPUTS        20     /* 패킹 Tally용 (VMIX_MAX_CHANNELS와 동일) */

/* ============================================================================
 * Tally 상태 값
 *
 * vMix TCP API 응답과 동일:
 *   - '0' (0x30) = Off
 *   - '1' (0x31) = Program (On Air)
 *   - '2' (0x32) = Preview
 *
 * 내부 저장은 ATEM과 동일한 형식 사용:
 *   - 0 = Off
 *   - 1 = Program
 *   - 2 = Preview
 *   - 3 = Both (Program + Preview)
 * ============================================================================ */

#define VMIX_TALLY_OFF               0      /* Off */
#define VMIX_TALLY_PROGRAM           1      /* Program (On Air) */
#define VMIX_TALLY_PREVIEW           2      /* Preview */
#define VMIX_TALLY_BOTH              3      /* Program + Preview (내부용) */

/* ============================================================================
 * 응답 상태
 * ============================================================================ */

#define VMIX_RESPONSE_OK             "OK"   /* 성공 */
#define VMIX_RESPONSE_ERROR          "ER"   /* 오류 */

/* ============================================================================
 * 명령 문자열
 * ============================================================================ */

/* 조회 명령 */
#define VMIX_CMD_TALLY               "TALLY"
#define VMIX_CMD_XML                 "XML"
#define VMIX_CMD_XMLTEXT             "XMLTEXT"
#define VMIX_CMD_ACTS                "ACTS"
#define VMIX_CMD_VERSION             "VERSION"

/* 구독 명령 */
#define VMIX_CMD_SUBSCRIBE           "SUBSCRIBE"
#define VMIX_CMD_UNSUBSCRIBE         "UNSUBSCRIBE"

/* 기능 실행 */
#define VMIX_CMD_FUNCTION            "FUNCTION"
#define VMIX_CMD_QUIT                "QUIT"

/* ============================================================================
 * FUNCTION 명령 (제어)
 * ============================================================================ */

/* 트랜지션 */
#define VMIX_FUNC_CUT                "Cut"
#define VMIX_FUNC_FADE               "Fade"
#define VMIX_FUNC_QUICK_PLAY         "QuickPlay"

/* 입력 선택 */
#define VMIX_FUNC_PREVIEW_INPUT      "PreviewInput"
#define VMIX_FUNC_ACTIVE_INPUT       "ActiveInput"      /* Program */
#define VMIX_FUNC_TRANSITION1        "Transition1"
#define VMIX_FUNC_TRANSITION2        "Transition2"
#define VMIX_FUNC_TRANSITION3        "Transition3"
#define VMIX_FUNC_TRANSITION4        "Transition4"

/* 오버레이 */
#define VMIX_FUNC_OVERLAY1_IN        "OverlayInput1In"
#define VMIX_FUNC_OVERLAY1_OUT       "OverlayInput1Out"
#define VMIX_FUNC_OVERLAY2_IN        "OverlayInput2In"
#define VMIX_FUNC_OVERLAY2_OUT       "OverlayInput2Out"
#define VMIX_FUNC_OVERLAY3_IN        "OverlayInput3In"
#define VMIX_FUNC_OVERLAY3_OUT       "OverlayInput3Out"
#define VMIX_FUNC_OVERLAY4_IN        "OverlayInput4In"
#define VMIX_FUNC_OVERLAY4_OUT       "OverlayInput4Out"

/* 재생 */
#define VMIX_FUNC_PLAY               "Play"
#define VMIX_FUNC_PAUSE              "Pause"
#define VMIX_FUNC_PLAY_PAUSE         "PlayPause"

/* ============================================================================
 * 유틸리티 함수
 * ============================================================================ */

/**
 * vMix Tally 문자를 숫자로 변환
 *
 * @param c vMix Tally 문자 ('0', '1', '2')
 * @return Tally 상태 값 (0, 1, 2)
 */
static inline uint8_t vmix_tally_char_to_value(char c)
{
    if (c == '1') return VMIX_TALLY_PROGRAM;
    if (c == '2') return VMIX_TALLY_PREVIEW;
    return VMIX_TALLY_OFF;
}

/**
 * 패킹된 Tally에서 채널 상태 조회
 *
 * @param packed 패킹된 Tally (64비트)
 * @param index 채널 인덱스 (0부터 시작)
 * @return Tally 상태 값
 */
static inline uint8_t vmix_tally_get(uint64_t packed, uint8_t index)
{
    if (index >= VMIX_MAX_CHANNELS) return 0;
    return (packed >> (index * 2)) & 0x03;
}

/**
 * 패킹된 Tally에 채널 상태 설정
 *
 * @param packed 패킹된 Tally 포인터
 * @param index 채널 인덱스 (0부터 시작)
 * @param value Tally 상태 값
 */
static inline void vmix_tally_set(uint64_t* packed, uint8_t index, uint8_t value)
{
    if (index >= VMIX_MAX_CHANNELS) return;
    uint8_t shift = index * 2;
    *packed &= ~((uint64_t)0x03 << shift);
    *packed |= ((uint64_t)(value & 0x03) << shift);
}

#ifdef __cplusplus
}
#endif

#endif /* VMIX_PROTOCOL_H */
