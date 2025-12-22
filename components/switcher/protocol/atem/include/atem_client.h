/**
 * ATEM 클라이언트
 *
 * ATEM 스위처 제어를 위한 메인 클라이언트 API
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef ATEM_CLIENT_H
#define ATEM_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "atem_protocol.h"
#include "atem_state.h"
#include "sw_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 클라이언트 구조체
 * ============================================================================ */

typedef struct {
    /* 네트워크 */
    sw_socket_t socket;
    char ip[16];
    uint16_t port;

    /* 상태 */
    atem_state_t state;

    /* 버퍼 */
    uint8_t rx_buffer[ATEM_RX_BUFFER_SIZE];
    uint8_t tx_buffer[ATEM_TX_BUFFER_SIZE];

    /* 콜백 */
    void (*on_connected)(void* user_data);
    void (*on_disconnected)(void* user_data);
    void (*on_state_changed)(const char* cmd_name, void* user_data);
    void* user_data;

    /* 디버그 */
    bool debug;
} atem_client_t;

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

/**
 * 클라이언트 초기화
 *
 * @param client 클라이언트 구조체 포인터
 * @param ip ATEM IP 주소
 * @param port 포트 번호 (기본: 9910)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_init(atem_client_t* client, const char* ip, uint16_t port);

/**
 * 클라이언트 정리
 *
 * @param client 클라이언트 구조체 포인터
 */
void atem_client_cleanup(atem_client_t* client);

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

/**
 * ATEM 스위처에 연결
 *
 * @param client 클라이언트 구조체 포인터
 * @param timeout_ms 연결 타임아웃 (ms)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_connect(atem_client_t* client, uint32_t timeout_ms);

/**
 * ATEM 연결 시작 (논블로킹)
 *
 * @param client 클라이언트 구조체 포인터
 * @return 성공 시 0, 진행중 1, 실패 시 -1
 */
int atem_client_connect_start(atem_client_t* client);

/**
 * ATEM 연결 상태 확인 (논블로킹)
 *
 * @param client 클라이언트 구조체 포인터
 * @return 연결 완료 0, 진행중 1, 실패 시 -1
 */
int atem_client_connect_check(atem_client_t* client);

/**
 * 연결 종료
 *
 * @param client 클라이언트 구조체 포인터
 */
void atem_client_disconnect(atem_client_t* client);

/**
 * 초기화 완료 대기
 *
 * @param client 클라이언트 구조체 포인터
 * @param timeout_ms 타임아웃 (ms)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_wait_init(atem_client_t* client, uint32_t timeout_ms);

/**
 * 연결 상태 확인
 *
 * @param client 클라이언트 구조체 포인터
 * @return 연결되어 있으면 true
 */
bool atem_client_is_connected(const atem_client_t* client);

/**
 * 초기화 완료 여부
 *
 * @param client 클라이언트 구조체 포인터
 * @return 초기화 완료되었으면 true
 */
bool atem_client_is_initialized(const atem_client_t* client);

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

/**
 * 루프 처리 (메인 루프에서 호출 필수!)
 *
 * - 패킷 수신 및 처리
 * - Keepalive 전송
 * - 타임아웃 감지
 *
 * @param client 클라이언트 구조체 포인터
 * @return 처리된 패킷 수 (에러 시 -1)
 */
int atem_client_loop(atem_client_t* client);

/* ============================================================================
 * 기기 정보 조회
 * ============================================================================ */

/**
 * 프로토콜 버전 조회
 */
bool atem_client_get_version(const atem_client_t* client, uint8_t* major, uint8_t* minor);

/**
 * 제품명 조회
 */
const char* atem_client_get_product_name(const atem_client_t* client);

/**
 * 토폴로지 정보 조회
 */
uint8_t atem_client_get_num_sources(const atem_client_t* client);
uint8_t atem_client_get_num_mes(const atem_client_t* client);
uint8_t atem_client_get_num_dsks(const atem_client_t* client);
uint8_t atem_client_get_num_cameras(const atem_client_t* client);
uint8_t atem_client_get_num_supersources(const atem_client_t* client);

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

/**
 * Program 입력 소스 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @return 소스 ID
 */
uint16_t atem_client_get_program_input(const atem_client_t* client, uint8_t me);

/**
 * Preview 입력 소스 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @return 소스 ID
 */
uint16_t atem_client_get_preview_input(const atem_client_t* client, uint8_t me);

/**
 * 소스가 Program인지 확인
 */
bool atem_client_is_program(const atem_client_t* client, uint16_t source_id, uint8_t me);

/**
 * 소스가 Preview인지 확인
 */
bool atem_client_is_preview(const atem_client_t* client, uint16_t source_id, uint8_t me);

/* ============================================================================
 * Tally 조회
 *
 * 상태값 (ATEM 프로토콜과 동일):
 *   0 = Off, 1 = Program (bit0), 2 = Preview (bit1), 3 = Both
 * ============================================================================ */

/**
 * Tally 상태 조회 (인덱스 기반)
 *
 * @param client 클라이언트 구조체 포인터
 * @param index 채널 인덱스 (0=CAM1, 1=CAM2, ...)
 * @return Tally 상태 값 (ATEM_TALLY_OFF/PROGRAM/PREVIEW/BOTH)
 */
uint8_t atem_client_get_tally_by_index(const atem_client_t* client, uint8_t index);

/**
 * 패킹된 Tally 전체 조회
 *
 * 변경 감지 시 효율적: if (current != prev) { ... }
 *
 * @param client 클라이언트 구조체 포인터
 * @return 64비트 패킹된 Tally (채널당 2비트 × 20채널)
 */
uint64_t atem_client_get_tally_packed(const atem_client_t* client);

/* ============================================================================
 * Transition 조회
 * ============================================================================ */

/**
 * 트랜지션 스타일 조회
 */
uint8_t atem_client_get_transition_style(const atem_client_t* client, uint8_t me);

/**
 * 트랜지션 위치 조회 (0-10000)
 */
uint16_t atem_client_get_transition_position(const atem_client_t* client, uint8_t me);

/**
 * 트랜지션 진행 중 확인
 */
bool atem_client_is_in_transition(const atem_client_t* client, uint8_t me);

/**
 * 트랜지션 프리뷰 활성화 여부
 */
bool atem_client_is_transition_preview_enabled(const atem_client_t* client, uint8_t me);

/* ============================================================================
 * Keyer 조회
 * ============================================================================ */

/**
 * ME의 Keyer 수 조회
 */
uint8_t atem_client_get_num_keyers(const atem_client_t* client, uint8_t me);

/**
 * Upstream Keyer On Air 상태
 */
bool atem_client_is_keyer_on_air(const atem_client_t* client, uint8_t me, uint8_t keyer_index);

/**
 * Upstream Keyer가 Next Transition에 포함되어 있는지 (Tie)
 */
bool atem_client_is_keyer_in_next(const atem_client_t* client, uint8_t me, uint8_t keyer_index);

/**
 * DSK On Air 상태
 */
bool atem_client_is_dsk_on_air(const atem_client_t* client, uint8_t dsk_index);

/**
 * DSK 트랜지션 진행 중
 */
bool atem_client_is_dsk_in_transition(const atem_client_t* client, uint8_t dsk_index);

/**
 * DSK Tie 상태 조회
 */
bool atem_client_is_dsk_tie(const atem_client_t* client, uint8_t dsk_index);

/**
 * SuperSource Fill 소스
 */
uint16_t atem_client_get_supersource_fill(const atem_client_t* client);

/**
 * SuperSource Key 소스
 */
uint16_t atem_client_get_supersource_key(const atem_client_t* client);

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

/**
 * Cut 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_cut(atem_client_t* client, uint8_t me);

/**
 * Auto (트랜지션) 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_auto(atem_client_t* client, uint8_t me);

/**
 * Program 입력 변경
 *
 * @param client 클라이언트 구조체 포인터
 * @param source_id 소스 ID
 * @param me ME 인덱스 (기본: 0)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_program_input(atem_client_t* client, uint16_t source_id, uint8_t me);

/**
 * Preview 입력 변경
 *
 * @param client 클라이언트 구조체 포인터
 * @param source_id 소스 ID
 * @param me ME 인덱스 (기본: 0)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_preview_input(atem_client_t* client, uint16_t source_id, uint8_t me);

/**
 * DSK On Air 설정
 *
 * @param client 클라이언트 구조체 포인터
 * @param dsk_index DSK 인덱스 (0부터 시작)
 * @param on_air true=On, false=Off
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_dsk_on_air(atem_client_t* client, uint8_t dsk_index, bool on_air);

/**
 * DSK Auto 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @param dsk_index DSK 인덱스 (0부터 시작)
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_dsk_auto(atem_client_t* client, uint8_t dsk_index);

/**
 * DSK Tie 설정
 *
 * @param client 클라이언트 구조체 포인터
 * @param dsk_index DSK 인덱스 (0부터 시작)
 * @param tie true=Tie On, false=Tie Off
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_dsk_tie(atem_client_t* client, uint8_t dsk_index, bool tie);

/**
 * USK (Upstream Keyer) On Air 설정
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @param keyer_index Keyer 인덱스 (0부터 시작)
 * @param on_air true=On, false=Off
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_keyer_on_air(atem_client_t* client, uint8_t me, uint8_t keyer_index, bool on_air);

/**
 * USK (Upstream Keyer)를 Next Transition에 포함/제외 설정
 *
 * @param client 클라이언트 구조체 포인터
 * @param me ME 인덱스 (기본: 0)
 * @param keyer_index Keyer 인덱스 (0부터 시작)
 * @param in_next true=포함, false=제외
 * @return 성공 시 0, 실패 시 -1
 */
int atem_client_set_keyer_in_next(atem_client_t* client, uint8_t me, uint8_t keyer_index, bool in_next);

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

/**
 * 연결 성공 콜백 설정
 */
void atem_client_set_on_connected(atem_client_t* client, void (*callback)(void*), void* user_data);

/**
 * 연결 종료 콜백 설정
 */
void atem_client_set_on_disconnected(atem_client_t* client, void (*callback)(void*), void* user_data);

/**
 * 상태 변경 콜백 설정
 */
void atem_client_set_on_state_changed(atem_client_t* client, void (*callback)(const char*, void*), void* user_data);

/* ============================================================================
 * 디버그
 * ============================================================================ */

/**
 * 디버그 모드 설정
 */
void atem_client_set_debug(atem_client_t* client, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* ATEM_CLIENT_H */
