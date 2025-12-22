/**
 * @file obs_client.h
 * @brief OBS WebSocket 클라이언트 API
 *
 * OBS Studio와 WebSocket 프로토콜(obs-websocket v5.x)을 통해 통신합니다.
 *
 * 사용법:
 * 1. obs_client_init() 으로 초기화
 * 2. obs_client_connect() 로 연결
 * 3. 메인 루프에서 obs_client_loop() 호출
 * 4. obs_client_cleanup() 으로 정리
 */

#ifndef OBS_CLIENT_H
#define OBS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "obs_protocol.h"
#include "obs_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 타입 정의
 *===========================================================================*/

/** 콜백 함수 타입 */
typedef void (*obs_callback_t)(void* user_data);

/**
 * 디버그 레벨 (ESP-IDF esp_log 레벨과 매핑)
 */
typedef enum {
    OBS_DEBUG_NONE = 0,
    OBS_DEBUG_ERROR = 1,
    OBS_DEBUG_INFO = 2,
    OBS_DEBUG_VERBOSE = 3
} obs_debug_level_t;

/*===========================================================================
 * 클라이언트 구조체
 *===========================================================================*/

/**
 * @brief OBS 클라이언트 구조체
 */
typedef struct {
    /* 연결 정보 */
    char host[64];                   /**< OBS 호스트 주소 */
    uint16_t port;                   /**< OBS 포트 */
    char password[64];               /**< OBS WebSocket 비밀번호 */

    /* 소켓 */
    int socket_fd;                   /**< 소켓 파일 디스크립터 */

    /* 상태 */
    obs_state_t state;               /**< 클라이언트 상태 */

    /* 콜백 */
    obs_callback_t on_connected;     /**< 연결 완료 콜백 */
    void* on_connected_data;
    obs_callback_t on_disconnected;  /**< 연결 해제 콜백 */
    void* on_disconnected_data;
    obs_callback_t on_scene_changed; /**< Scene 변경 콜백 */
    void* on_scene_changed_data;
    obs_callback_t on_authenticated; /**< 인증 완료 콜백 */
    void* on_authenticated_data;

    /* 디버그 */
    obs_debug_level_t debug_level;   /**< 디버그 레벨 */

    /* 내부 WebSocket 클라이언트용 예약 공간 */
    /* ws_client_t: ~3.8KB (2KB recv + 1KB frame + 512B send + misc) */
    uint8_t _ws_reserved[4096];      /**< WebSocket 클라이언트 내부 데이터 */

} obs_client_t;

/*===========================================================================
 * 초기화 및 정리
 *===========================================================================*/

/**
 * @brief 클라이언트 초기화
 * @param client 클라이언트 구조체 포인터
 * @param host OBS 호스트 주소
 * @param port OBS 포트 (기본값: OBS_DEFAULT_PORT)
 * @param password OBS WebSocket 비밀번호 (NULL 또는 빈 문자열: 인증 없음)
 * @return 0 성공, -1 실패
 */
int obs_client_init(obs_client_t* client, const char* host, uint16_t port, const char* password);

/**
 * @brief 클라이언트 정리
 * @param client 클라이언트 구조체 포인터
 */
void obs_client_cleanup(obs_client_t* client);

/*===========================================================================
 * 연결 관리
 *===========================================================================*/

/**
 * @brief OBS에 연결
 * @param client 클라이언트 구조체 포인터
 * @param timeout_ms 연결 타임아웃 (ms)
 * @return 0 성공, -1 실패
 */
int obs_client_connect(obs_client_t* client, uint32_t timeout_ms);

/**
 * @brief OBS 연결 시작 (논블로킹)
 * @param client 클라이언트 구조체 포인터
 * @return 0 성공, 1 진행중, -1 실패
 */
int obs_client_connect_start(obs_client_t* client);

/**
 * @brief OBS 연결 상태 확인 (논블로킹)
 * @param client 클라이언트 구조체 포인터
 * @return 0 연결 완료, 1 진행중, -1 실패
 */
int obs_client_connect_check(obs_client_t* client);

/**
 * @brief OBS 연결 해제
 * @param client 클라이언트 구조체 포인터
 */
void obs_client_disconnect(obs_client_t* client);

/**
 * @brief 연결 상태 확인
 * @param client 클라이언트 구조체 포인터
 * @return true 연결됨 (인증 완료), false 끊김
 */
bool obs_client_is_connected(const obs_client_t* client);

/**
 * @brief 초기화 완료 확인
 * @param client 클라이언트 구조체 포인터
 * @return true 초기화 완료 (Scene List 파싱됨), false 진행중
 */
bool obs_client_is_initialized(const obs_client_t* client);

/**
 * @brief 초기화 완료 대기
 * @param client 클라이언트 구조체 포인터
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 0, 실패 시 -1
 */
int obs_client_wait_init(obs_client_t* client, uint32_t timeout_ms);

/**
 * @brief 메인 루프 처리
 * @param client 클라이언트 구조체 포인터
 * @return 0 성공, -1 오류
 *
 * 주기적으로 호출하여 WebSocket 메시지를 처리합니다.
 */
int obs_client_loop(obs_client_t* client);

/*===========================================================================
 * 상태 조회
 *===========================================================================*/

/**
 * @brief 현재 Program Scene 인덱스 가져오기
 * @param client 클라이언트 구조체 포인터
 * @return Scene 인덱스 (0-based), -1: 없음
 */
int16_t obs_client_get_program_scene(const obs_client_t* client);

/**
 * @brief 현재 Preview Scene 인덱스 가져오기
 * @param client 클라이언트 구조체 포인터
 * @return Scene 인덱스 (0-based), -1: 없음 또는 Studio Mode 비활성
 */
int16_t obs_client_get_preview_scene(const obs_client_t* client);

/**
 * @brief Scene 개수 가져오기
 * @param client 클라이언트 구조체 포인터
 * @return Scene 개수
 */
uint8_t obs_client_get_scene_count(const obs_client_t* client);

/**
 * @brief Scene 이름 가져오기
 * @param client 클라이언트 구조체 포인터
 * @param index Scene 인덱스
 * @return Scene 이름 (NULL: 없음)
 */
const char* obs_client_get_scene_name(const obs_client_t* client, uint8_t index);

/**
 * @brief 특정 Scene의 Tally 가져오기
 * @param client 클라이언트 구조체 포인터
 * @param index Scene 인덱스
 * @return Tally 값 (OBS_TALLY_OFF, OBS_TALLY_PROGRAM, OBS_TALLY_PREVIEW)
 */
uint8_t obs_client_get_tally_by_index(const obs_client_t* client, uint8_t index);

/**
 * @brief Packed Tally 가져오기
 * @param client 클라이언트 구조체 포인터
 * @return 64비트 packed tally (32채널 × 2비트)
 */
uint64_t obs_client_get_tally_packed(const obs_client_t* client);

/**
 * @brief Studio Mode 상태 가져오기
 * @param client 클라이언트 구조체 포인터
 * @return true Studio Mode 활성, false 비활성
 */
bool obs_client_is_studio_mode(const obs_client_t* client);

/*===========================================================================
 * 제어
 *===========================================================================*/

/**
 * @brief Program Scene 변경 (인덱스)
 * @param client 클라이언트 구조체 포인터
 * @param index Scene 인덱스
 * @return 0 성공, -1 실패
 */
int obs_client_set_program_scene(obs_client_t* client, uint8_t index);

/**
 * @brief Program Scene 변경 (이름)
 * @param client 클라이언트 구조체 포인터
 * @param name Scene 이름
 * @return 0 성공, -1 실패
 */
int obs_client_set_program_scene_by_name(obs_client_t* client, const char* name);

/**
 * @brief Preview Scene 변경 (인덱스, Studio Mode 전용)
 * @param client 클라이언트 구조체 포인터
 * @param index Scene 인덱스
 * @return 0 성공, -1 실패
 */
int obs_client_set_preview_scene(obs_client_t* client, uint8_t index);

/**
 * @brief Preview Scene 변경 (이름, Studio Mode 전용)
 * @param client 클라이언트 구조체 포인터
 * @param name Scene 이름
 * @return 0 성공, -1 실패
 */
int obs_client_set_preview_scene_by_name(obs_client_t* client, const char* name);

/**
 * @brief Scene 목록 새로고침 요청
 * @param client 클라이언트 구조체 포인터
 * @return 0 성공, -1 실패
 */
int obs_client_refresh_scenes(obs_client_t* client);

/**
 * @brief Cut - Preview를 Program으로 즉시 전환 (Studio Mode)
 *
 * Studio Mode가 아닌 경우 set_program_scene과 동일하게 동작
 *
 * @param client 클라이언트 구조체 포인터
 * @return 0 성공, -1 실패
 */
int obs_client_cut(obs_client_t* client);

/**
 * @brief Auto - Preview를 Program으로 트랜지션 (Studio Mode)
 *
 * Studio Mode에서 현재 설정된 트랜지션으로 전환
 *
 * @param client 클라이언트 구조체 포인터
 * @return 0 성공, -1 실패
 */
int obs_client_auto(obs_client_t* client);

/**
 * @brief Studio Mode 설정
 * @param client 클라이언트 구조체 포인터
 * @param enabled true=활성화, false=비활성화
 * @return 0 성공, -1 실패
 */
int obs_client_set_studio_mode(obs_client_t* client, bool enabled);

/*===========================================================================
 * 콜백 설정
 *===========================================================================*/

/**
 * @brief 연결 완료 콜백 설정
 */
void obs_client_set_on_connected(obs_client_t* client, obs_callback_t callback, void* user_data);

/**
 * @brief 연결 해제 콜백 설정
 */
void obs_client_set_on_disconnected(obs_client_t* client, obs_callback_t callback, void* user_data);

/**
 * @brief Scene 변경 콜백 설정
 */
void obs_client_set_on_scene_changed(obs_client_t* client, obs_callback_t callback, void* user_data);

/**
 * @brief 인증 완료 콜백 설정
 */
void obs_client_set_on_authenticated(obs_client_t* client, obs_callback_t callback, void* user_data);

/*===========================================================================
 * 디버그
 *===========================================================================*/

/**
 * @brief 디버그 레벨 설정
 * @param client 클라이언트 구조체 포인터
 * @param level 디버그 레벨
 */
void obs_client_set_debug(obs_client_t* client, obs_debug_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* OBS_CLIENT_H */
