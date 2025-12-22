/**
 * Switcher 통합 핸들러
 *
 * 여러 종류의 스위처를 통합 인터페이스로 제어
 * - ATEM (Blackmagic Design)
 * - vMix
 * - OBS
 * - OSEE
 *
 * 순수 C 언어로 작성 (C99)
 */

#ifndef SWITCHER_H
#define SWITCHER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "switcher_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Tally 상태 (공통)
 * ============================================================================ */

#define SWITCHER_TALLY_OFF      0   /* Off */
#define SWITCHER_TALLY_PROGRAM  1   /* Program (On Air) */
#define SWITCHER_TALLY_PREVIEW  2   /* Preview */
#define SWITCHER_TALLY_BOTH     3   /* Program + Preview */

/* ============================================================================
 * 에러 코드
 * ============================================================================ */

typedef enum {
    SWITCHER_OK = 0,
    SWITCHER_ERROR = -1,
    SWITCHER_ERROR_INVALID_PARAM = -2,
    SWITCHER_ERROR_NOT_CONNECTED = -3,
    SWITCHER_ERROR_TIMEOUT = -4,
    SWITCHER_ERROR_NOT_SUPPORTED = -5,
    SWITCHER_ERROR_NOT_INITIALIZED = -6
} switcher_error_t;

/* ============================================================================
 * 설정
 * ============================================================================ */

#define SWITCHER_MAX_CHANNELS    20   /* Tally 패킹 최대 채널 수 (ATEM/OBS/vMix 공통) */
#define SWITCHER_MAX_SOURCES     64   /* 최대 소스 수 */
#define SWITCHER_MAX_NAME_LEN    64   /* 이름 최대 길이 */
#define SWITCHER_IP_LEN          16   /* IP 주소 길이 */
#define SWITCHER_PASSWORD_LEN    64   /* 비밀번호 최대 길이 (OBS용) */

/* ============================================================================
 * 스위처 정보 (공통)
 * ============================================================================ */

typedef struct {
    char product_name[SWITCHER_MAX_NAME_LEN];
    uint8_t num_cameras;  /* 카메라 개수 (ATEM: num_inputs, OBS/vMix: scene count) */
    uint8_t num_mes;      /* Mix Effect 수 (ATEM 전용) */
} switcher_info_t;

/* ============================================================================
 * 스위처 상태 (공통)
 * ============================================================================ */

typedef struct {
    bool connected;
    bool initialized;

    /* Program/Preview */
    uint16_t program_input;
    uint16_t preview_input;

    /* Tally (패킹된 형태) */
    uint64_t tally_packed;

    /* 트랜지션 */
    bool in_transition;
    uint16_t transition_position;  /* 0-10000 */
} switcher_state_t;

/* ============================================================================
 * 콜백 타입
 * ============================================================================ */

typedef void (*switcher_on_connected_t)(void* user_data);
typedef void (*switcher_on_disconnected_t)(void* user_data);
typedef void (*switcher_on_tally_changed_t)(uint64_t tally_packed, void* user_data);
typedef void (*switcher_on_state_changed_t)(const char* what, void* user_data);

/* ============================================================================
 * 콜백 구조체
 * ============================================================================ */

typedef struct {
    switcher_on_connected_t on_connected;
    switcher_on_disconnected_t on_disconnected;
    switcher_on_tally_changed_t on_tally_changed;
    switcher_on_state_changed_t on_state_changed;
    void* user_data;
} switcher_callbacks_t;

/* ============================================================================
 * 스위처 핸들 (불투명 포인터)
 * ============================================================================ */

typedef struct switcher_handle switcher_t;

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

/**
 * 스위처 생성
 *
 * @param type 스위처 타입
 * @param ip IP 주소
 * @param port 포트 번호 (0이면 기본값 사용)
 * @return 스위처 핸들, 실패 시 NULL
 */
switcher_t* switcher_create(switcher_type_t type, const char* ip, uint16_t port);

/**
 * 스위처 생성 (비밀번호 포함)
 *
 * OBS WebSocket 등 비밀번호가 필요한 스위처용
 *
 * @param type 스위처 타입
 * @param ip IP 주소
 * @param port 포트 번호 (0이면 기본값 사용)
 * @param password 비밀번호 (NULL이면 비밀번호 없음)
 * @return 스위처 핸들, 실패 시 NULL
 */
switcher_t* switcher_create_with_password(switcher_type_t type, const char* ip, uint16_t port, const char* password);

/**
 * 스위처 해제
 *
 * @param sw 스위처 핸들
 */
void switcher_destroy(switcher_t* sw);

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

/**
 * 스위처 연결
 *
 * @param sw 스위처 핸들
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 SWITCHER_OK
 */
int switcher_connect(switcher_t* sw, uint32_t timeout_ms);

/**
 * 스위처 연결 시작 (논블로킹)
 *
 * @param sw 스위처 핸들
 * @return 성공 시 SWITCHER_OK, 진행중 1, 실패 시 SWITCHER_ERROR
 */
int switcher_connect_start(switcher_t* sw);

/**
 * 스위처 연결 상태 확인 (논블로킹)
 *
 * @param sw 스위처 핸들
 * @return 연결 완료 시 SWITCHER_OK, 진행중 1, 실패 시 SWITCHER_ERROR
 */
int switcher_connect_check(switcher_t* sw);

/**
 * 스위처 연결 해제
 *
 * @param sw 스위처 핸들
 */
void switcher_disconnect(switcher_t* sw);

/**
 * 연결 상태 확인
 *
 * @param sw 스위처 핸들
 * @return 연결되어 있으면 true
 */
bool switcher_is_connected(const switcher_t* sw);

/**
 * 초기화 완료 여부 확인
 *
 * @param sw 스위처 핸들
 * @return 초기화 완료 시 true
 */
bool switcher_is_initialized(const switcher_t* sw);

/**
 * 초기화 완료 대기
 *
 * @param sw 스위처 핸들
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 SWITCHER_OK
 */
int switcher_wait_init(switcher_t* sw, uint32_t timeout_ms);

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

/**
 * 루프 처리 (메인 루프에서 호출 필수!)
 *
 * @param sw 스위처 핸들
 * @return 처리된 이벤트 수, 에러 시 -1
 */
int switcher_loop(switcher_t* sw);

/* ============================================================================
 * 정보 조회
 * ============================================================================ */

/**
 * 스위처 타입 조회
 *
 * @param sw 스위처 핸들
 * @return 스위처 타입
 */
switcher_type_t switcher_get_type(const switcher_t* sw);

/**
 * 스위처 타입 이름 조회
 *
 * @param type 스위처 타입
 * @return 타입 이름 문자열
 */
const char* switcher_type_name(switcher_type_t type);

/**
 * 스위처 정보 조회
 *
 * @param sw 스위처 핸들
 * @param info 정보를 저장할 구조체
 * @return 성공 시 SWITCHER_OK
 */
int switcher_get_info(const switcher_t* sw, switcher_info_t* info);

/**
 * 스위처 상태 조회
 *
 * @param sw 스위처 핸들
 * @param state 상태를 저장할 구조체
 * @return 성공 시 SWITCHER_OK
 */
int switcher_get_state(const switcher_t* sw, switcher_state_t* state);

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

/**
 * Program 입력 조회
 *
 * @param sw 스위처 핸들
 * @return 소스 ID
 */
uint16_t switcher_get_program(const switcher_t* sw);

/**
 * Preview 입력 조회
 *
 * @param sw 스위처 핸들
 * @return 소스 ID
 */
uint16_t switcher_get_preview(const switcher_t* sw);

/* ============================================================================
 * Tally 조회
 * ============================================================================ */

/**
 * Tally 상태 조회 (인덱스 기반)
 *
 * @param sw 스위처 핸들
 * @param index 채널 인덱스 (0부터 시작)
 * @return Tally 상태 (SWITCHER_TALLY_*)
 */
uint8_t switcher_get_tally(const switcher_t* sw, uint8_t index);

/**
 * 패킹된 Tally 조회
 *
 * @param sw 스위처 핸들
 * @return 64비트 패킹된 Tally
 */
uint64_t switcher_get_tally_packed(const switcher_t* sw);

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

/**
 * Cut 실행
 *
 * @param sw 스위처 핸들
 * @return 성공 시 SWITCHER_OK
 */
int switcher_cut(switcher_t* sw);

/**
 * Auto (트랜지션) 실행
 *
 * @param sw 스위처 핸들
 * @return 성공 시 SWITCHER_OK
 */
int switcher_auto(switcher_t* sw);

/**
 * Program 입력 변경
 *
 * @param sw 스위처 핸들
 * @param input 소스 ID
 * @return 성공 시 SWITCHER_OK
 */
int switcher_set_program(switcher_t* sw, uint16_t input);

/**
 * Preview 입력 변경
 *
 * @param sw 스위처 핸들
 * @param input 소스 ID
 * @return 성공 시 SWITCHER_OK
 */
int switcher_set_preview(switcher_t* sw, uint16_t input);

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

/**
 * 콜백 설정
 *
 * @param sw 스위처 핸들
 * @param callbacks 콜백 구조체
 */
void switcher_set_callbacks(switcher_t* sw, const switcher_callbacks_t* callbacks);

/* ============================================================================
 * 디버그
 * ============================================================================ */

/**
 * 디버그 모드 설정
 *
 * @param sw 스위처 핸들
 * @param enable true면 활성화
 */
void switcher_set_debug(switcher_t* sw, bool enable);

/* ============================================================================
 * 정보 출력
 * ============================================================================ */

/**
 * 스위처 토폴로지 출력 (printf 사용)
 *
 * 연결된 스위처의 모든 정보를 출력
 * - 제품명, 프로토콜 버전
 * - 입력/ME/DSK/Keyer 수
 * - 현재 PGM/PVW 상태
 * - Tally 상태
 *
 * @param sw 스위처 핸들
 */
void switcher_print_topology(const switcher_t* sw);

/**
 * 현재 상태 출력 (printf 사용)
 *
 * PGM/PVW 및 Tally 상태만 간단히 출력
 *
 * @param sw 스위처 핸들
 */
void switcher_print_status(const switcher_t* sw);

/* ============================================================================
 * 유틸리티
 * ============================================================================ */

/**
 * 패킹된 Tally에서 채널 상태 추출
 *
 * @param packed 패킹된 Tally
 * @param index 채널 인덱스
 * @return Tally 상태
 */
static inline uint8_t switcher_tally_get(uint64_t packed, uint8_t index)
{
    if (index >= SWITCHER_MAX_CHANNELS) return 0;
    return (packed >> (index * 2)) & 0x03;
}

/**
 * 패킹된 Tally에 채널 상태 설정
 *
 * @param packed 패킹된 Tally 포인터
 * @param index 채널 인덱스
 * @param value Tally 상태
 */
static inline void switcher_tally_set(uint64_t* packed, uint8_t index, uint8_t value)
{
    if (index >= SWITCHER_MAX_CHANNELS) return;
    uint8_t shift = index * 2;
    *packed &= ~((uint64_t)0x03 << shift);
    *packed |= ((uint64_t)(value & 0x03) << shift);
}

/**
 * 패킹된 Tally를 PGM/PVW 배열로 언팩
 *
 * BOTH 상태인 채널은 PGM과 PVW 모두에 포함됨
 *
 * @param sw 스위처 핸들
 * @param pgm PGM 채널 배열 (1-based, 최대 20개)
 * @param pgm_count PGM 채널 수 반환
 * @param pvw PVW 채널 배열 (1-based, 최대 20개)
 * @param pvw_count PVW 채널 수 반환
 */
void switcher_tally_unpack(const switcher_t* sw,
                           uint8_t* pgm, uint8_t* pgm_count,
                           uint8_t* pvw, uint8_t* pvw_count);

/**
 * PGM/PVW를 문자열로 포맷
 *
 * 출력 예: "PGM: 1,2 / PVW: 3"
 *
 * @param sw 스위처 핸들
 * @param buf 출력 버퍼
 * @param buf_size 버퍼 크기 (최소 64바이트 권장)
 * @return buf 포인터
 */
char* switcher_tally_format(const switcher_t* sw, char* buf, size_t buf_size);

/* ============================================================================
 * 카메라 매핑 설정
 * ============================================================================ */

/**
 * 카메라 제한 설정
 *
 * 사용자가 이 스위처에서 사용할 최대 카메라 개수를 설정합니다.
 * 예: ATEM이 20개 카메라를 보고하지만 5개만 사용하고 싶을 때
 *
 * @param sw 스위처 핸들
 * @param limit 최대 카메라 개수 (0 = 제한 없음)
 * @return 성공 시 SWITCHER_OK
 */
int switcher_set_camera_limit(switcher_t* sw, uint8_t limit);

/**
 * 카메라 오프셋 설정
 *
 * RX로 전송 시 카메라 번호에 더할 오프셋을 설정합니다.
 * 예: OBS Scene 1~6을 RX의 카메라 6~11로 매핑하려면 offset=5
 *
 * @param sw 스위처 핸들
 * @param offset 카메라 번호 오프셋 (기본 0)
 * @return 성공 시 SWITCHER_OK
 */
int switcher_set_camera_offset(switcher_t* sw, uint8_t offset);

/**
 * 카메라 제한 조회
 *
 * @param sw 스위처 핸들
 * @return 사용자 설정 카메라 제한 (0 = 제한 없음)
 */
uint8_t switcher_get_camera_limit(const switcher_t* sw);

/**
 * 카메라 오프셋 조회
 *
 * @param sw 스위처 핸들
 * @return 카메라 번호 오프셋
 */
uint8_t switcher_get_camera_offset(const switcher_t* sw);

/**
 * 유효 카메라 개수 조회
 *
 * 실제로 사용되는 카메라 개수를 반환합니다.
 * min(user_limit, switcher_reported, hardware_max)
 *
 * @param sw 스위처 핸들
 * @return 유효 카메라 개수
 */
uint8_t switcher_get_effective_camera_count(const switcher_t* sw);

#ifdef __cplusplus
}
#endif

#endif /* SWITCHER_H */
