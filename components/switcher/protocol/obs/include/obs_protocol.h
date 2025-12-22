/**
 * @file obs_protocol.h
 * @brief OBS WebSocket 프로토콜 정의 (obs-websocket v5.x)
 *
 * OBS Studio WebSocket 프로토콜 상수 및 구조체 정의
 */

#ifndef OBS_PROTOCOL_H
#define OBS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "switcher_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 연결 설정 (switcher_config.h에서 공통 관리)
 *===========================================================================*/

/** 기본 WebSocket 포트 */
#define OBS_DEFAULT_PORT            4455

/** 연결 타임아웃 (ms) */
#define OBS_CONNECT_TIMEOUT_MS      SWITCHER_CONNECT_TIMEOUT_MS

/** 인증 타임아웃 (ms) */
#define OBS_AUTH_TIMEOUT_MS         3000

/** 최대 무응답 시간 (ms) */
#define OBS_MAX_SILENCE_TIME_MS     SWITCHER_MAX_SILENCE_TIME_MS

/** Keepalive(Ping) 전송 간격 (ms) - OBS는 10초 (WebSocket Heartbeat) */
#define OBS_KEEPALIVE_INTERVAL_MS   10000

/** 재연결 간격 (ms) */
#define OBS_RECONNECT_INTERVAL_MS   SWITCHER_RECONNECT_INTERVAL_MS

/*===========================================================================
 * OpCode (Operation Code)
 *===========================================================================*/

/** Hello - 서버 → 클라이언트 (연결 시 인증 정보) */
#define OBS_OP_HELLO                0

/** Identify - 클라이언트 → 서버 (인증 및 구독) */
#define OBS_OP_IDENTIFY             1

/** Identified - 서버 → 클라이언트 (인증 완료) */
#define OBS_OP_IDENTIFIED           2

/** Reidentify - 클라이언트 → 서버 (재인증) */
#define OBS_OP_REIDENTIFY           3

/** Event - 서버 → 클라이언트 (이벤트 알림) */
#define OBS_OP_EVENT                5

/** Request - 클라이언트 → 서버 (요청) */
#define OBS_OP_REQUEST              6

/** RequestResponse - 서버 → 클라이언트 (요청 응답) */
#define OBS_OP_REQUEST_RESPONSE     7

/** RequestBatch - 클라이언트 → 서버 (배치 요청) */
#define OBS_OP_REQUEST_BATCH        8

/** RequestBatchResponse - 서버 → 클라이언트 (배치 응답) */
#define OBS_OP_REQUEST_BATCH_RESPONSE 9

/*===========================================================================
 * Close Code (연결 종료 코드)
 *===========================================================================*/

/** 알 수 없는 이유로 종료 */
#define OBS_CLOSE_UNKNOWN           4000

/** 클라이언트가 Identify 전송 전에 다른 메시지 전송 */
#define OBS_CLOSE_MESSAGE_DECODE_ERROR 4002

/** 클라이언트가 잘못된 Identify 메시지 전송 */
#define OBS_CLOSE_MISSING_DATA_FIELD 4003

/** 클라이언트가 잘못된 데이터 필드 전송 */
#define OBS_CLOSE_INVALID_DATA_FIELD 4004

/** 인증 실패 */
#define OBS_CLOSE_AUTH_FAILED       4009

/** 지원되지 않는 RPC 버전 */
#define OBS_CLOSE_UNSUPPORTED_RPC   4010

/*===========================================================================
 * Event Subscription (이벤트 구독 비트마스크)
 *===========================================================================*/

/** 없음 */
#define OBS_EVENT_NONE              0

/** General 이벤트 (종료, StudioMode 변경 등) */
#define OBS_EVENT_GENERAL           (1 << 0)

/** Config 이벤트 */
#define OBS_EVENT_CONFIG            (1 << 1)

/** Scenes 이벤트 (Scene 변경) */
#define OBS_EVENT_SCENES            (1 << 2)

/** Inputs 이벤트 */
#define OBS_EVENT_INPUTS            (1 << 3)

/** Transitions 이벤트 */
#define OBS_EVENT_TRANSITIONS       (1 << 4)

/** Filters 이벤트 */
#define OBS_EVENT_FILTERS           (1 << 5)

/** Outputs 이벤트 */
#define OBS_EVENT_OUTPUTS           (1 << 6)

/** SceneItems 이벤트 */
#define OBS_EVENT_SCENE_ITEMS       (1 << 7)

/** MediaInputs 이벤트 */
#define OBS_EVENT_MEDIA_INPUTS      (1 << 8)

/** Vendors 이벤트 */
#define OBS_EVENT_VENDORS           (1 << 9)

/** UI 이벤트 */
#define OBS_EVENT_UI                (1 << 10)

/** 모든 이벤트 */
#define OBS_EVENT_ALL               0xFFFF

/** Tally에 필요한 이벤트 (General + Scenes) */
#define OBS_EVENT_TALLY             (OBS_EVENT_GENERAL | OBS_EVENT_SCENES)

/*===========================================================================
 * Tally 값
 *===========================================================================*/

/** Tally Off */
#define OBS_TALLY_OFF               0

/** Tally Program (On Air) */
#define OBS_TALLY_PROGRAM           1

/** Tally Preview */
#define OBS_TALLY_PREVIEW           2

/*===========================================================================
 * 제한값
 *===========================================================================*/

/** Tally 패킹 최대 채널 수 (SWITCHER_MAX_CHANNELS와 동일) */
#define OBS_MAX_CHANNELS            20

/** 최대 Scene 수 (OBS_MAX_CHANNELS와 동일) */
#define OBS_MAX_SCENES              20

/** Scene 이름 최대 길이 (ESP32 최적화: 64자) */
#define OBS_SCENE_NAME_MAX          64

/** 전송 버퍼 크기 */
#define OBS_SEND_BUFFER_SIZE        512

/** 인증 문자열 최대 길이 */
#define OBS_AUTH_STRING_MAX         64

/** Request ID 최대 길이 */
#define OBS_REQUEST_ID_MAX          32

/*===========================================================================
 * RPC 버전
 *===========================================================================*/

/** 현재 지원 RPC 버전 */
#define OBS_RPC_VERSION             1

/*===========================================================================
 * Request Type 문자열
 *===========================================================================*/

#define OBS_REQUEST_GET_SCENE_LIST          "GetSceneList"
#define OBS_REQUEST_GET_CURRENT_PROGRAM     "GetCurrentProgramScene"
#define OBS_REQUEST_SET_CURRENT_PROGRAM     "SetCurrentProgramScene"
#define OBS_REQUEST_GET_CURRENT_PREVIEW     "GetCurrentPreviewScene"
#define OBS_REQUEST_SET_CURRENT_PREVIEW     "SetCurrentPreviewScene"
#define OBS_REQUEST_GET_STUDIO_MODE         "GetStudioModeEnabled"
#define OBS_REQUEST_SET_STUDIO_MODE         "SetStudioModeEnabled"
#define OBS_REQUEST_TRIGGER_TRANSITION      "TriggerStudioModeTransition"

/*===========================================================================
 * Event Type 문자열
 *===========================================================================*/

#define OBS_EVENT_CURRENT_PROGRAM_CHANGED   "CurrentProgramSceneChanged"
#define OBS_EVENT_CURRENT_PREVIEW_CHANGED   "CurrentPreviewSceneChanged"
#define OBS_EVENT_STUDIO_MODE_CHANGED       "StudioModeStateChanged"
#define OBS_EVENT_SCENE_LIST_CHANGED        "SceneListChanged"

/*===========================================================================
 * 유틸리티 함수
 *===========================================================================*/

/**
 * @brief Tally packed 값에서 특정 Scene의 Tally 가져오기
 * @param packed 64비트 packed tally
 * @param scene_index Scene 인덱스 (0-31)
 * @return Tally 값 (OBS_TALLY_OFF, OBS_TALLY_PROGRAM, OBS_TALLY_PREVIEW)
 */
static inline uint8_t obs_tally_get(uint64_t packed, uint8_t scene_index)
{
    if (scene_index >= OBS_MAX_CHANNELS) return OBS_TALLY_OFF;
    return (packed >> (scene_index * 2)) & 0x03;
}

/**
 * @brief Tally packed 값에 특정 Scene의 Tally 설정
 * @param packed 64비트 packed tally 포인터
 * @param scene_index Scene 인덱스 (0-31)
 * @param tally Tally 값
 */
static inline void obs_tally_set(uint64_t* packed, uint8_t scene_index, uint8_t tally)
{
    if (scene_index >= OBS_MAX_CHANNELS || !packed) return;
    uint64_t mask = ~((uint64_t)0x03 << (scene_index * 2));
    *packed = (*packed & mask) | ((uint64_t)(tally & 0x03) << (scene_index * 2));
}

/**
 * @brief Program/Preview 인덱스에서 Tally packed 값 생성
 * @param program_index Program Scene 인덱스 (-1: 없음)
 * @param preview_index Preview Scene 인덱스 (-1: 없음)
 * @return 64비트 packed tally
 */
static inline uint64_t obs_tally_pack(int16_t program_index, int16_t preview_index)
{
    uint64_t packed = 0;
    if (program_index >= 0 && program_index < OBS_MAX_CHANNELS) {
        obs_tally_set(&packed, (uint8_t)program_index, OBS_TALLY_PROGRAM);
    }
    if (preview_index >= 0 && preview_index < OBS_MAX_CHANNELS) {
        /* Preview는 Program이 아닌 경우에만 설정 */
        if (preview_index != program_index) {
            obs_tally_set(&packed, (uint8_t)preview_index, OBS_TALLY_PREVIEW);
        }
    }
    return packed;
}

#ifdef __cplusplus
}
#endif

#endif /* OBS_PROTOCOL_H */
