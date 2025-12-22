/**
 * ATEM 상태 구조체 정의
 *
 * ATEM 스위처의 상태를 저장하는 구조체들
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef ATEM_STATE_H
#define ATEM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "atem_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Transition 상태
 * ============================================================================ */

typedef struct {
    uint8_t style;           /* 트랜지션 스타일 */
    uint16_t position;       /* 트랜지션 위치 (0-10000) */
    bool in_transition;      /* 트랜지션 진행 중 */
    bool preview_enabled;    /* 트랜지션 프리뷰 활성화 */
    bool next_background;    /* Next: Background */
    uint8_t next_key;        /* Next: Key 비트마스크 (bit0=Key1, bit1=Key2...) */
} atem_transition_state_t;

/* ============================================================================
 * Keyer 상태
 * ============================================================================ */

typedef struct {
    bool on_air;             /* On Air 상태 */
} atem_keyer_state_t;

/* ============================================================================
 * DSK 상태
 * ============================================================================ */

typedef struct {
    bool on_air;             /* On Air 상태 */
    bool in_transition;      /* 트랜지션 진행 중 */
    bool tie;                /* Tie 상태 */
} atem_dsk_state_t;

/* ============================================================================
 * Input 정보
 * ============================================================================ */

typedef struct {
    uint16_t source_id;                              /* 소스 ID */
    char long_name[ATEM_INPUT_LONG_NAME_LEN];        /* 긴 이름 (최대 20자) */
    char short_name[ATEM_INPUT_SHORT_NAME_LEN];      /* 짧은 이름 (최대 4자) */
    bool valid;                                      /* 유효한 항목 */
} atem_input_info_t;

/* ============================================================================
 * ATEM 전체 상태
 * ============================================================================ */

typedef struct {
    /* 연결 상태 */
    bool connected;
    bool initialized;
    uint16_t session_id;
    uint32_t last_contact_ms;

    /* 패킷 ID 추적 */
    uint16_t local_packet_id;
    uint16_t remote_packet_id;
    uint16_t last_received_packet_id;

    /* 초기화 추적 */
    bool init_payload_sent;
    uint16_t init_payload_sent_at_packet_id;

    /* Keepalive */
    uint32_t last_keepalive_ms;

    /* 기기 정보 */
    uint8_t protocol_major;
    uint8_t protocol_minor;
    char product_name[ATEM_PRODUCT_NAME_LEN];

    /* 토폴로지 */
    uint8_t num_sources;
    uint8_t num_mes;
    uint8_t num_dsks;
    uint8_t num_cameras;
    uint8_t num_supersources;

    /* Program/Preview (ME별) */
    uint16_t program_input[ATEM_MAX_MES];
    uint16_t preview_input[ATEM_MAX_MES];

    /* Tally (패킹된 형태)
     *
     * 20채널 × 2비트 = 40비트, uint64_t 사용
     * 각 채널: bit0=Program, bit1=Preview
     *
     * 해독: atem_tally_get(tally_packed, index)
     * 비교: if (current != prev) → 변경됨
     */
    uint64_t tally_packed;

    /* Tally 원본 (디버그/호환용) */
    uint8_t tally_raw[ATEM_MAX_CHANNELS];
    uint8_t tally_raw_count;

    /* Transition (ME별) */
    atem_transition_state_t transition[ATEM_MAX_MES];

    /* ME Config (Keyer 수) */
    uint8_t num_keyers[ATEM_MAX_MES];

    /* Keyer 상태 (ME * Keyer) */
    atem_keyer_state_t keyers[ATEM_MAX_MES * ATEM_MAX_KEYERS];
    uint8_t keyer_count;

    /* DSK 상태 */
    atem_dsk_state_t dsks[ATEM_MAX_DSKS];

    /* SuperSource */
    uint16_t supersource_fill;
    uint16_t supersource_key;

    /* Input 정보 */
    atem_input_info_t inputs[ATEM_MAX_INPUTS];
    uint8_t input_count;

    /* 카메라 제한 및 매핑 */
    uint8_t user_camera_limit;       /* 사용자 설정 (0 = 제한 없음) */
    uint8_t camera_offset;           /* RX 전송 시 카메라 번호 오프셋 (기본 0) */
    uint8_t effective_camera_limit;  /* 실제 사용되는 제한 */

    /* Tally 모드 */
    uint8_t tally_mode;              /* 1=직접, 2=계산 */
    bool tally_needs_update;         /* 캐시 업데이트 필요 */
} atem_state_t;

/* ============================================================================
 * 상태 초기화 함수
 * ============================================================================ */

/**
 * 유효한 카메라 제한 재계산
 *
 * 정책:
 * 1. 하드 제한: ATEM_MAX_CHANNELS (20채널)
 * 2. 사용자 제한이 0(오토)이면 → num_cameras 사용
 * 3. 사용자 제한이 있으면 → min(user_camera_limit, num_cameras) 사용
 * 4. 모든 경우 하드 제한(20채널)을 초과하지 않음
 *
 * @param state 상태 구조체 포인터
 */
static inline void atem_state_update_camera_limit(atem_state_t* state)
{
    uint8_t limit = ATEM_MAX_CHANNELS;  /* 하드 제한 20채널 */

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
 * ATEM 상태 초기화
 *
 * @param state 상태 구조체 포인터
 */
static inline void atem_state_init(atem_state_t* state)
{
    /* offset과 limit 보존 */
    uint8_t saved_offset = state->camera_offset;
    uint8_t saved_limit = state->user_camera_limit;

    memset(state, 0, sizeof(atem_state_t));
    state->tally_mode = 1;  /* 기본: 직접 모드 */
    state->effective_camera_limit = ATEM_MAX_CHANNELS;

    /* 보존된 값 복원 */
    state->camera_offset = saved_offset;
    state->user_camera_limit = saved_limit;

    /* 복원된 user_camera_limit을 반영하여 effective_camera_limit 재계산 */
    atem_state_update_camera_limit(state);
}

#ifdef __cplusplus
}
#endif

#endif /* ATEM_STATE_H */
