/**
 * @file obs_state.h
 * @brief OBS 클라이언트 상태 구조체
 */

#ifndef OBS_STATE_H
#define OBS_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "obs_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scene 정보 구조체
 */
typedef struct {
    char name[OBS_SCENE_NAME_MAX];  /**< Scene 이름 */
    int index;                       /**< Scene 인덱스 */
} obs_scene_info_t;

/**
 * @brief OBS 클라이언트 상태 구조체
 */
typedef struct {
    /* 연결 상태 */
    bool connected;                  /**< WebSocket 연결됨 */
    bool authenticated;              /**< 인증 완료 */
    bool initialized;                /**< 초기화 완료 (Scene List 파싱됨) */
    bool studio_mode;                /**< Studio Mode 활성화 */

    /* 타이밍 */
    uint32_t last_contact_ms;        /**< 마지막 메시지 수신 시간 */
    uint32_t last_keepalive_ms;      /**< 마지막 keepalive 전송 시간 */
    uint32_t connect_start_ms;       /**< 연결 시작 시간 */

    /* Scene 상태 */
    int16_t program_scene_index;     /**< 현재 Program Scene 인덱스 (-1: 없음) */
    int16_t preview_scene_index;     /**< 현재 Preview Scene 인덱스 (-1: 없음) */
    char program_scene_name[OBS_SCENE_NAME_MAX];  /**< Program Scene 이름 */
    char preview_scene_name[OBS_SCENE_NAME_MAX];  /**< Preview Scene 이름 */

    /* Scene 목록 */
    obs_scene_info_t scenes[OBS_MAX_SCENES];  /**< Scene 목록 */
    uint8_t num_cameras;                       /**< Scene 개수 (카메라 개수로 간주) */

    /* Tally */
    uint64_t tally_packed;           /**< 32채널 × 2비트 packed tally */

    /* 인증 정보 (서버에서 수신) */
    char challenge[OBS_AUTH_STRING_MAX];  /**< 인증 challenge */
    char salt[OBS_AUTH_STRING_MAX];       /**< 인증 salt */
    bool auth_required;                    /**< 인증 필요 여부 */

    /* Request ID 관리 */
    uint32_t next_request_id;        /**< 다음 요청 ID */

    /* 카메라 제한 및 매핑 */
    uint8_t user_camera_limit;       /* 사용자 설정 (0 = 제한 없음) */
    uint8_t camera_offset;           /* RX 전송 시 카메라 번호 오프셋 (기본 0) */
    uint8_t effective_camera_limit;  /* 실제 사용되는 제한 */

    /* 수신 버퍼는 WebSocket 레이어에서 관리 (중복 제거) */

} obs_state_t;

/**
 * @brief 상태 초기화
 * @param state 상태 구조체 포인터
 */
/**
 * @brief 유효한 카메라 제한 재계산
 *
 * 정책:
 * 1. 하드 제한: OBS_MAX_CHANNELS (20채널)
 * 2. 사용자 제한이 0(오토)이면 → num_cameras 사용
 * 3. 사용자 제한이 있으면 → min(user_camera_limit, num_cameras) 사용
 * 4. 모든 경우 하드 제한(20채널)을 초과하지 않음
 *
 * @param state 상태 구조체 포인터
 */
static inline void obs_state_update_camera_limit(obs_state_t* state)
{
    if (!state) return;

    uint8_t limit = OBS_MAX_CHANNELS;  /* 하드 제한 20채널 */

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
 * @brief 상태 초기화
 * @param state 상태 구조체 포인터
 */
static inline void obs_state_init(obs_state_t* state)
{
    if (!state) return;

    /* offset과 limit 보존 */
    uint8_t saved_offset = state->camera_offset;
    uint8_t saved_limit = state->user_camera_limit;

    state->connected = false;
    state->authenticated = false;
    state->initialized = false;
    state->studio_mode = false;

    state->last_contact_ms = 0;
    state->last_keepalive_ms = 0;
    state->connect_start_ms = 0;

    state->program_scene_index = -1;
    state->preview_scene_index = -1;
    state->program_scene_name[0] = '\0';
    state->preview_scene_name[0] = '\0';

    state->num_cameras = 0;
    for (int i = 0; i < OBS_MAX_SCENES; i++) {
        state->scenes[i].name[0] = '\0';
        state->scenes[i].index = -1;
    }

    state->tally_packed = 0;

    state->challenge[0] = '\0';
    state->salt[0] = '\0';
    state->auth_required = false;

    state->next_request_id = 1;

    state->effective_camera_limit = OBS_MAX_CHANNELS;

    /* 보존된 값 복원 */
    state->camera_offset = saved_offset;
    state->user_camera_limit = saved_limit;

    /* 복원된 user_camera_limit을 반영하여 effective_camera_limit 재계산 */
    obs_state_update_camera_limit(state);
}

/**
 * @brief Tally 업데이트 (Program/Preview 인덱스 기반)
 * @param state 상태 구조체 포인터
 */
static inline void obs_state_update_tally(obs_state_t* state)
{
    if (!state) return;
    state->tally_packed = obs_tally_pack(
        state->program_scene_index,
        state->preview_scene_index
    );
}

/**
 * @brief Scene 이름으로 인덱스 찾기
 * @param state 상태 구조체 포인터
 * @param name Scene 이름
 * @return Scene 인덱스 (-1: 없음)
 */
static inline int16_t obs_state_find_scene_index(const obs_state_t* state, const char* name)
{
    if (!state || !name) return -1;

    for (uint8_t i = 0; i < state->num_cameras; i++) {
        /* strcmp 대신 수동 비교 (헤더에서 사용) */
        const char* a = state->scenes[i].name;
        const char* b = name;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == *b) {
            return (int16_t)i;
        }
    }
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* OBS_STATE_H */
