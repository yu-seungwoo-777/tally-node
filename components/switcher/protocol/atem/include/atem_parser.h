/**
 * ATEM 명령 파서
 *
 * ATEM 패킷에서 명령을 추출하고 파싱하는 함수들
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef ATEM_PARSER_H
#define ATEM_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include "atem_protocol.h"
#include "atem_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 파서 콜백 타입
 * ============================================================================ */

/**
 * 명령 콜백 함수 타입
 *
 * @param cmd_name 명령 이름 (4글자)
 * @param cmd_data 명령 데이터 (cmd_header 이후)
 * @param cmd_length 명령 데이터 길이
 * @param user_data 사용자 데이터
 */
typedef void (*atem_command_callback_t)(const char* cmd_name,
                                        const uint8_t* cmd_data,
                                        uint16_t cmd_length,
                                        void* user_data);

/* ============================================================================
 * 파서 함수
 * ============================================================================ */

/**
 * 패킷에서 명령 추출 및 처리
 *
 * @param data 패킷 데이터 (헤더 포함)
 * @param length 패킷 길이
 * @param callback 명령 콜백 함수
 * @param user_data 콜백에 전달할 사용자 데이터
 * @return 추출된 명령 수
 */
int atem_parse_commands(const uint8_t* data, uint16_t length,
                        atem_command_callback_t callback, void* user_data);

/**
 * 명령 파싱 및 상태 업데이트
 *
 * @param state 상태 구조체
 * @param cmd_name 명령 이름
 * @param cmd_data 명령 데이터
 * @param cmd_length 명령 데이터 길이
 */
void atem_update_state(atem_state_t* state, const char* cmd_name,
                       const uint8_t* cmd_data, uint16_t cmd_length);

/* ============================================================================
 * 개별 명령 파서 (내부 사용)
 * ============================================================================ */

void atem_parse_version(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_product_id(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_topology(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_me_config(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_tally_config(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_input_prop(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_program_input(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_preview_input(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_state_update_tally(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_tally_source(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_transition_settings(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_transition_position(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_transition_preview(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_keyer_on_air(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_dsk_state(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_dsk_properties(atem_state_t* state, const uint8_t* data, uint16_t length);
void atem_parse_supersource(atem_state_t* state, const uint8_t* data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* ATEM_PARSER_H */
