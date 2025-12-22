/**
 * vMix 클라이언트
 *
 * vMix 스위처 제어를 위한 메인 클라이언트 API
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef VMIX_CLIENT_H
#define VMIX_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "vmix_protocol.h"
#include "vmix_state.h"
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
    vmix_state_t state;

    /* 버퍼 */
    uint8_t rx_buffer[VMIX_RX_BUFFER_SIZE];
    uint8_t tx_buffer[VMIX_TX_BUFFER_SIZE];

    /* 콜백 */
    void (*on_connected)(void* user_data);
    void (*on_disconnected)(void* user_data);
    void (*on_tally_changed)(void* user_data);
    void* user_data;

    /* 디버그 */
    bool debug;
} vmix_client_t;

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

/**
 * 클라이언트 초기화
 *
 * @param client 클라이언트 구조체 포인터
 * @param ip vMix IP 주소
 * @param port 포트 번호 (기본: 8099)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_init(vmix_client_t* client, const char* ip, uint16_t port);

/**
 * 클라이언트 정리
 *
 * @param client 클라이언트 구조체 포인터
 */
void vmix_client_cleanup(vmix_client_t* client);

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

/**
 * vMix 서버에 연결
 *
 * @param client 클라이언트 구조체 포인터
 * @param timeout_ms 연결 타임아웃 (ms)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_connect(vmix_client_t* client, uint32_t timeout_ms);

/**
 * vMix 연결 시작 (논블로킹)
 *
 * @param client 클라이언트 구조체 포인터
 * @return 성공 시 0, 진행중 1, 실패 시 -1
 */
int vmix_client_connect_start(vmix_client_t* client);

/**
 * vMix 연결 상태 확인 (논블로킹)
 *
 * @param client 클라이언트 구조체 포인터
 * @return 연결 완료 0, 진행중 1, 실패 시 -1
 */
int vmix_client_connect_check(vmix_client_t* client);

/**
 * 연결 종료
 *
 * @param client 클라이언트 구조체 포인터
 */
void vmix_client_disconnect(vmix_client_t* client);

/**
 * 연결 상태 확인
 *
 * @param client 클라이언트 구조체 포인터
 * @return 연결되어 있으면 true
 */
bool vmix_client_is_connected(const vmix_client_t* client);

/**
 * 초기화 완료 여부 확인
 *
 * vMix는 TALLY 구독 완료 + 첫 TALLY 데이터 수신 후 초기화 완료
 *
 * @param client 클라이언트 구조체 포인터
 * @return TALLY 구독 완료 및 데이터 수신 시 true
 */
bool vmix_client_is_initialized(const vmix_client_t* client);

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

/**
 * 루프 처리 (메인 루프에서 호출 필수!)
 *
 * - TCP 데이터 수신 및 처리
 * - Tally 업데이트 감지
 * - 타임아웃 감지
 *
 * @param client 클라이언트 구조체 포인터
 * @return 처리된 이벤트 수 (에러 시 -1)
 */
int vmix_client_loop(vmix_client_t* client);

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

/**
 * Program 입력 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @return 입력 번호 (1부터 시작, 0=없음)
 */
uint16_t vmix_client_get_program_input(const vmix_client_t* client);

/**
 * Preview 입력 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @return 입력 번호 (1부터 시작, 0=없음)
 */
uint16_t vmix_client_get_preview_input(const vmix_client_t* client);

/* ============================================================================
 * Tally 조회
 * ============================================================================ */

/**
 * Tally 상태 조회 (인덱스 기반)
 *
 * @param client 클라이언트 구조체 포인터
 * @param index 채널 인덱스 (0부터 시작)
 * @return Tally 상태 값 (VMIX_TALLY_OFF/PROGRAM/PREVIEW)
 */
uint8_t vmix_client_get_tally_by_index(const vmix_client_t* client, uint8_t index);

/**
 * 패킹된 Tally 전체 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @return 64비트 패킹된 Tally
 */
uint64_t vmix_client_get_tally_packed(const vmix_client_t* client);

/**
 * Tally 개수 조회
 *
 * @param client 클라이언트 구조체 포인터
 * @return Tally 개수
 */
uint8_t vmix_client_get_tally_count(const vmix_client_t* client);

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

/**
 * Cut 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_cut(vmix_client_t* client);

/**
 * Fade 트랜지션 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_fade(vmix_client_t* client);

/**
 * Preview 입력 변경
 *
 * @param client 클라이언트 구조체 포인터
 * @param input 입력 번호 (1부터 시작)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_set_preview_input(vmix_client_t* client, uint16_t input);

/**
 * Program (Active) 입력 변경
 *
 * @param client 클라이언트 구조체 포인터
 * @param input 입력 번호 (1부터 시작)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_set_program_input(vmix_client_t* client, uint16_t input);

/**
 * QuickPlay 실행
 *
 * @param client 클라이언트 구조체 포인터
 * @param input 입력 번호 (1부터 시작)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_quick_play(vmix_client_t* client, uint16_t input);

/**
 * 오버레이 On
 *
 * @param client 클라이언트 구조체 포인터
 * @param overlay_index 오버레이 인덱스 (1-4)
 * @param input 입력 번호 (1부터 시작)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_overlay_in(vmix_client_t* client, uint8_t overlay_index, uint16_t input);

/**
 * 오버레이 Off
 *
 * @param client 클라이언트 구조체 포인터
 * @param overlay_index 오버레이 인덱스 (1-4)
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_overlay_out(vmix_client_t* client, uint8_t overlay_index);

/**
 * FUNCTION 명령 전송 (범용)
 *
 * @param client 클라이언트 구조체 포인터
 * @param function 기능 이름 (예: "Cut", "Fade")
 * @param params 추가 파라미터 (예: "Input=2") 또는 NULL
 * @return 성공 시 0, 실패 시 -1
 */
int vmix_client_function(vmix_client_t* client, const char* function, const char* params);

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

/**
 * 연결 성공 콜백 설정
 */
void vmix_client_set_on_connected(vmix_client_t* client, void (*callback)(void*), void* user_data);

/**
 * 연결 종료 콜백 설정
 */
void vmix_client_set_on_disconnected(vmix_client_t* client, void (*callback)(void*), void* user_data);

/**
 * Tally 변경 콜백 설정
 */
void vmix_client_set_on_tally_changed(vmix_client_t* client, void (*callback)(void*), void* user_data);

/* ============================================================================
 * 디버그
 * ============================================================================ */

/**
 * 디버그 모드 설정
 */
void vmix_client_set_debug(vmix_client_t* client, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* VMIX_CLIENT_H */
