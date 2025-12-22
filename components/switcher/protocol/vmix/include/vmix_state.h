/**
 * vMix 상태 구조체 정의
 *
 * vMix 스위처의 상태를 저장하는 구조체들
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef VMIX_STATE_H
#define VMIX_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vmix_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * vMix 전체 상태
 * ============================================================================ */

typedef struct {
    /* 연결 상태 */
    bool connected;
    bool subscribed;              /* TALLY 구독 완료 */
    uint32_t last_contact_ms;
    uint32_t last_keepalive_ms;   /* 마지막 keepalive 전송 시간 */

    /* Tally (패킹된 형태)
     *
     * 32채널 × 2비트 = 64비트, uint64_t 사용
     * 각 채널: 0=Off, 1=Program, 2=Preview
     *
     * 해독: vmix_tally_get(tally_packed, index)
     * 비교: if (current != prev) → 변경됨
     */
    uint64_t tally_packed;

    /* Tally 원본 (디버그용) */
    uint8_t tally_raw[VMIX_MAX_INPUTS];
    uint8_t num_cameras;          /* 실제 Tally 개수 (카메라 개수로 간주) */

    /* Program/Preview (첫 번째 발견된 것) */
    uint16_t program_input;       /* 1부터 시작, 0=없음 */
    uint16_t preview_input;       /* 1부터 시작, 0=없음 */

    /* 수신 버퍼 (라인 파싱용) */
    char line_buffer[VMIX_LINE_BUFFER_SIZE];
    uint16_t line_pos;

    /* 카메라 제한 및 매핑 */
    uint8_t user_camera_limit;       /* 사용자 설정 (0 = 제한 없음) */
    uint8_t camera_offset;           /* RX 전송 시 카메라 번호 오프셋 (기본 0) */
    uint8_t effective_camera_limit;  /* 실제 사용되는 제한 */

} vmix_state_t;

/* ============================================================================
 * 상태 초기화 함수
 * ============================================================================ */

/**
 * vMix 상태 초기화
 *
 * @param state 상태 구조체 포인터
 */
/**
 * 유효한 카메라 제한 재계산
 *
 * 정책:
 * 1. 하드 제한: VMIX_MAX_CHANNELS (20채널)
 * 2. 사용자 제한이 0(오토)이면 → num_cameras 사용
 * 3. 사용자 제한이 있으면 → min(user_camera_limit, num_cameras) 사용
 * 4. 모든 경우 하드 제한(20채널)을 초과하지 않음
 *
 * @param state 상태 구조체 포인터
 */
static inline void vmix_state_update_camera_limit(vmix_state_t* state)
{
    uint8_t limit = VMIX_MAX_CHANNELS;  /* 하드 제한 20채널 */

    /* 사용자 제한이 0이면 num_cameras 사용 (오토 모드) */
    if (state->user_camera_limit == 0) {
        /* num_cameras가 있고 하드 제한보다 작으면 num_cameras 적용 */
        if (state->num_cameras > 0 && state->num_cameras < limit) {
            limit = state->num_cameras;
        }
    }
    /* 사용자 제한이 있으면 사용자 제한과 num_cameras 중 작은 값 */
    else {
        /* 사용자 제한 적용 */
        if (state->user_camera_limit < limit) {
            limit = state->user_camera_limit;
        }
        /* num_cameras가 더 작으면 num_cameras로 제한 */
        if (state->num_cameras > 0 && state->num_cameras < limit) {
            limit = state->num_cameras;
        }
    }

    state->effective_camera_limit = limit;
}

/**
 * vMix 상태 초기화
 *
 * @param state 상태 구조체 포인터
 */
static inline void vmix_state_init(vmix_state_t* state)
{
    /* offset과 limit 보존 */
    uint8_t saved_offset = state->camera_offset;
    uint8_t saved_limit = state->user_camera_limit;

    memset(state, 0, sizeof(vmix_state_t));
    state->effective_camera_limit = VMIX_MAX_CHANNELS;

    /* 보존된 값 복원 */
    state->camera_offset = saved_offset;
    state->user_camera_limit = saved_limit;

    /* 복원된 user_camera_limit을 반영하여 effective_camera_limit 재계산 */
    vmix_state_update_camera_limit(state);
}

/**
 * Tally 데이터 업데이트 (문자열에서)
 *
 * @param state 상태 구조체 포인터
 * @param tally_str vMix Tally 문자열 ("01200...")
 * @param len 문자열 길이
 */
static inline void vmix_state_update_tally(vmix_state_t* state, const char* tally_str, uint16_t len)
{
    if (!state || !tally_str) return;

    /* Tally 개수 제한 */
    if (len > VMIX_MAX_INPUTS) len = VMIX_MAX_INPUTS;
    state->num_cameras = (uint8_t)len;

    /* 카메라 제한 업데이트 */
    vmix_state_update_camera_limit(state);

    /* 패킹 초기화 */
    state->tally_packed = 0;
    state->program_input = 0;
    state->preview_input = 0;

    /* effective_camera_limit 적용 */
    uint16_t effective_len = (len < state->effective_camera_limit) ? len : state->effective_camera_limit;

    /* 파싱 및 패킹 */
    for (uint16_t i = 0; i < effective_len; i++) {
        uint8_t val = vmix_tally_char_to_value(tally_str[i]);
        state->tally_raw[i] = val;

        /* 패킹 (최대 20채널) */
        if (i < VMIX_MAX_CHANNELS) {
            vmix_tally_set(&state->tally_packed, (uint8_t)i, val);
        }

        /* 첫 번째 Program/Preview 저장 */
        if (val == VMIX_TALLY_PROGRAM && state->program_input == 0) {
            state->program_input = i + 1;  /* 1부터 시작 */
        }
        if (val == VMIX_TALLY_PREVIEW && state->preview_input == 0) {
            state->preview_input = i + 1;  /* 1부터 시작 */
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* VMIX_STATE_H */
