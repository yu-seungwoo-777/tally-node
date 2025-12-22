/**
 * ATEM 명령 파서 구현
 *
 * ATEM 패킷에서 명령을 추출하고 파싱하는 함수들
 */

#include "atem_parser.h"
#include "atem_buffer.h"
#include "atem_debug.h"
#include "log.h"
#include "log_tags.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 패킷 파싱
 * ============================================================================ */

int atem_parse_commands(const uint8_t* data, uint16_t length,
                        atem_command_callback_t callback, void* user_data)
{
    if (length < ATEM_HEADER_LENGTH) {
        return 0;
    }

    int count = 0;
    uint16_t offset = ATEM_HEADER_LENGTH;

    while (offset + ATEM_CMD_HEADER_LENGTH <= length) {
        /* 명령 길이 (2바이트) */
        uint16_t cmd_length = atem_get_u16(data, offset);
        if (cmd_length < ATEM_CMD_HEADER_LENGTH) {
            break;  /* 잘못된 명령 */
        }

        if (offset + cmd_length > length) {
            break;  /* 패킷 범위 초과 */
        }

        /* 명령 이름 (4바이트, offset+4~+7) */
        char cmd_name[5];
        cmd_name[0] = (char)data[offset + 4];
        cmd_name[1] = (char)data[offset + 5];
        cmd_name[2] = (char)data[offset + 6];
        cmd_name[3] = (char)data[offset + 7];
        cmd_name[4] = '\0';

        /* 명령 데이터 (헤더 이후) */
        const uint8_t* cmd_data = data + offset + ATEM_CMD_HEADER_LENGTH;
        uint16_t cmd_data_length = cmd_length - ATEM_CMD_HEADER_LENGTH;

        /* 명령 로그 */
        ATEM_LOGV("CMD: %.4s (%d bytes)", cmd_name, cmd_data_length);

        /* 콜백 호출 */
        if (callback) {
            callback(cmd_name, cmd_data, cmd_data_length, user_data);
        }

        count++;
        offset += cmd_length;
    }

    return count;
}

/* ============================================================================
 * 상태 업데이트
 * ============================================================================ */

void atem_update_state(atem_state_t* state, const char* cmd_name,
                       const uint8_t* cmd_data, uint16_t cmd_length)
{
    /* 첫 글자로 분기하여 비교 횟수 최소화 */
    switch (cmd_name[0]) {
    case '_':  /* _ver, _top, _MeC, _TlC, _pin */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_VERSION)) {
            atem_parse_version(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TOPOLOGY)) {
            atem_parse_topology(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_ME_CONFIG)) {
            atem_parse_me_config(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TALLY_CONFIG)) {
            atem_parse_tally_config(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_PRODUCT_ID)) {
            atem_parse_product_id(state, cmd_data, cmd_length);
        }
        break;

    case 'P':  /* PrgI, PrvI, _pin */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_PROGRAM_INPUT)) {
            atem_parse_program_input(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_PREVIEW_INPUT)) {
            atem_parse_preview_input(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        }
        break;

    case 'T':  /* TlIn, TlSr, TrSS, TrPs, TrPr */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_TALLY_INDEX)) {
            atem_state_update_tally(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TALLY_SOURCE)) {
            atem_parse_tally_source(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TRANSITION_SETTINGS)) {
            atem_parse_transition_settings(state, cmd_data, cmd_length);
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TRANSITION_POSITION)) {
            atem_parse_transition_position(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_TRANSITION_PREVIEW)) {
            atem_parse_transition_preview(state, cmd_data, cmd_length);
        }
        break;

    case 'K':  /* KeOn */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_KEYER_ON_AIR)) {
            atem_parse_keyer_on_air(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        }
        break;

    case 'D':  /* DskS, DskP */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_DSK_STATE)) {
            atem_parse_dsk_state(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_DSK_PROPERTIES)) {
            atem_parse_dsk_properties(state, cmd_data, cmd_length);
        }
        break;

    case 'S':  /* SSrc */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_SUPERSOURCE)) {
            atem_parse_supersource(state, cmd_data, cmd_length);
            state->tally_needs_update = true;
        }
        break;

    case 'I':  /* InCm, InPr */
        if (atem_cmd_equals(cmd_name, ATEM_CMD_INIT_COMPLETE)) {
            state->initialized = true;
            ATEM_LOGI("초기화 완료");
        } else if (atem_cmd_equals(cmd_name, ATEM_CMD_INPUT_PROP)) {
            atem_parse_input_prop(state, cmd_data, cmd_length);
        }
        break;

    default:
        /* 처리하지 않는 명령 - 무시 */
        break;
    }
}

/* ============================================================================
 * 개별 명령 파서 구현
 * ============================================================================ */

void atem_parse_version(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 4) return;

    /* 버전 형식: major(2바이트) + minor(2바이트), big-endian */
    state->protocol_major = atem_get_u16(data, 0);
    state->protocol_minor = atem_get_u16(data, 2);

    ATEM_LOGI("프로토콜: %d.%d",
             state->protocol_major, state->protocol_minor);
}

void atem_parse_product_id(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 1) return;

    uint16_t max_len = length < ATEM_PRODUCT_NAME_LEN - 1 ? length : ATEM_PRODUCT_NAME_LEN - 1;
    atem_get_string(state->product_name, data, 0, max_len);

    ATEM_LOGI("제품명: %s", state->product_name);
}

void atem_parse_topology(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 10) return;

    /* _top 명령 구조:
     * offset 0: numMEs
     * offset 1: numSources
     * offset 5: numDSKs
     * offset 6: numSuperSources
     */
    state->num_mes = atem_get_u8(data, 0);
    state->num_sources = atem_get_u8(data, 1);
    state->num_dsks = atem_get_u8(data, 5);
    state->num_supersources = atem_get_u8(data, 6);

    ATEM_LOGI("토폴로지: ME=%d, 소스=%d, DSK=%d, SS=%d",
             state->num_mes, state->num_sources, state->num_dsks, state->num_supersources);
}

void atem_parse_me_config(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 2) return;

    uint8_t me = atem_get_u8(data, 0);
    uint8_t keyers = atem_get_u8(data, 1);

    if (me < ATEM_MAX_MES) {
        state->num_keyers[me] = keyers;
        ATEM_LOGV("ME%d 설정: Keyer=%d", me, keyers);
    }
}

void atem_parse_tally_config(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 5) return;

    state->num_cameras = atem_get_u8(data, 4);
    atem_state_update_camera_limit(state);

    ATEM_LOGV("Tally 설정: 카메라=%d", state->num_cameras);
}

void atem_parse_input_prop(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    /*
     * InPr (Input Properties) 구조:
     * offset 0-1: Source ID (uint16, big-endian)
     * offset 2-21: Long Name (20바이트)
     * offset 22-25: Short Name (4바이트)
     *
     * ATEM Mini는 36바이트, 다른 모델은 더 길 수 있음
     * 최소 26바이트면 이름 파싱 가능
     */
    if (length < 26) return;

    uint16_t source_id = atem_get_u16(data, 0);

    /* 빈 슬롯 찾기 또는 기존 항목 업데이트 */
    int slot = -1;
    for (uint8_t i = 0; i < state->input_count; i++) {
        if (state->inputs[i].source_id == source_id) {
            slot = i;  /* 기존 항목 */
            break;
        }
    }

    if (slot < 0) {
        /* 새 항목 추가 */
        if (state->input_count >= ATEM_MAX_INPUTS) {
            ATEM_LOGW("Input 저장 공간 부족: source_id=%d", source_id);
            return;
        }
        slot = state->input_count++;
    }

    /* 저장 */
    atem_input_info_t* input = &state->inputs[slot];
    input->source_id = source_id;
    input->valid = true;

    /* Long Name (offset 2, 20바이트) */
    atem_get_string(input->long_name, data, 2, ATEM_INPUT_LONG_NAME_LEN - 1);

    /* Short Name (offset 22, 4바이트) */
    atem_get_string(input->short_name, data, 22, ATEM_INPUT_SHORT_NAME_LEN - 1);

    ATEM_LOGV("Input %d: \"%s\" (%s)", source_id, input->long_name, input->short_name);
}

void atem_parse_program_input(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 4) return;

    uint8_t me = atem_get_u8(data, 0);
    uint16_t source = atem_get_u16(data, 2);

    if (me < ATEM_MAX_MES) {
        state->program_input[me] = source;
        ATEM_LOGV("ME%d Program: %d", me, source);
    }
}

void atem_parse_preview_input(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 4) return;

    uint8_t me = atem_get_u8(data, 0);
    uint16_t source = atem_get_u16(data, 2);

    if (me < ATEM_MAX_MES) {
        state->preview_input[me] = source;
        ATEM_LOGV("ME%d Preview: %d", me, source);
    }
}

void atem_state_update_tally(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 2) return;

    uint16_t count = atem_get_u16(data, 0);
    uint16_t max_count = (length - 2 < count) ? (length - 2) : count;

    if (max_count > ATEM_MAX_CHANNELS) {
        max_count = ATEM_MAX_CHANNELS;
    }

    /* 비트 플래그를 Tally 상태 값(0~3)으로 변환하여 패킹 저장 */
    state->tally_packed = 0;  /* 초기화 */
    state->tally_raw_count = max_count;

    LOG_1(TAG_ATEM, "Tally 파싱 시작 - %d개 소스", max_count);

    for (uint16_t i = 0; i < max_count; i++) {
        uint8_t flags = atem_get_u8(data, 2 + i);
        state->tally_raw[i] = flags;  /* 원본 저장 */
        uint8_t value = atem_tally_from_flags(flags);
        atem_tally_set(&state->tally_packed, i, value);

        if (i < 10 && value > 0) {  // 처음 10개와 활성화된 것만 출력
            LOG_1(TAG_ATEM, "  - [%2d] flags=0x%02X, value=%d (PGM=%s, PVW=%s)",
                      i + 1, flags, value,
                      (value == 2 || value == 3) ? "O" : "X",
                      (value == 1 || value == 3) ? "O" : "X");
        }
    }

    LOG_1(TAG_ATEM, "Tally 파싱 완료 - packed=0x%016llX", state->tally_packed);
}

void atem_parse_tally_source(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    /* TlSr: 소스 ID별 Tally 정보 */
    (void)state;
    (void)data;
    (void)length;
    /* 필요시 구현 */
}

void atem_parse_transition_settings(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    /*
     * TrSS (Transition Settings) 구조:
     * offset 0: ME 인덱스
     * offset 1: Transition 스타일 (0=Mix, 1=Dip, 2=Wipe, 3=DVE, 4=Sting)
     * offset 2: Next Transition Selection
     *           bit 0 = Background
     *           bit 1 = Key 1
     *           bit 2 = Key 2
     *           bit 3 = Key 3
     *           bit 4 = Key 4
     * offset 3: Style for Next Transition
     */
    if (length < 3) return;

    uint8_t me = atem_get_u8(data, 0);
    uint8_t style = atem_get_u8(data, 1);
    uint8_t next_selection = atem_get_u8(data, 2);

    if (me < ATEM_MAX_MES) {
        state->transition[me].style = style;
        state->transition[me].next_background = (next_selection & 0x01) != 0;
        state->transition[me].next_key = (next_selection >> 1) & 0x0F;  /* bit1-4 → bit0-3 */
        ATEM_LOGV("ME%d Transition: style=%d, next_bkgd=%d, next_key=0x%02X",
                  me, style, state->transition[me].next_background, state->transition[me].next_key);
    }
}

void atem_parse_transition_position(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 6) return;

    uint8_t me = atem_get_u8(data, 0);
    bool in_transition = atem_get_u8(data, 1) != 0;
    uint16_t position = atem_get_u16(data, 4);

    if (me < ATEM_MAX_MES) {
        state->transition[me].in_transition = in_transition;
        state->transition[me].position = position;
    }
}

void atem_parse_transition_preview(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 2) return;

    uint8_t me = atem_get_u8(data, 0);
    bool enabled = atem_get_u8(data, 1) != 0;

    if (me < ATEM_MAX_MES) {
        state->transition[me].preview_enabled = enabled;
    }
}

void atem_parse_keyer_on_air(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 3) return;

    uint8_t me = atem_get_u8(data, 0);
    uint8_t keyer = atem_get_u8(data, 1);
    bool on_air = atem_get_u8(data, 2) != 0;

    if (me < ATEM_MAX_MES && keyer < ATEM_MAX_KEYERS) {
        uint8_t index = me * ATEM_MAX_KEYERS + keyer;
        state->keyers[index].on_air = on_air;
        ATEM_LOGV("ME%d Keyer%d OnAir: %d", me, keyer, on_air);
    }
}

void atem_parse_dsk_state(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 3) return;

    uint8_t dsk = atem_get_u8(data, 0);
    bool on_air = atem_get_u8(data, 1) != 0;
    bool in_transition = atem_get_u8(data, 2) != 0;

    if (dsk < ATEM_MAX_DSKS) {
        state->dsks[dsk].on_air = on_air;
        state->dsks[dsk].in_transition = in_transition;
        ATEM_LOGV("DSK%d OnAir=%d, InTransition=%d", dsk, on_air, in_transition);
    }
}

void atem_parse_dsk_properties(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 2) return;

    uint8_t dsk = atem_get_u8(data, 0);
    bool tie = atem_get_u8(data, 1) != 0;

    if (dsk < ATEM_MAX_DSKS) {
        state->dsks[dsk].tie = tie;
        ATEM_LOGV("DSK%d Tie=%d", dsk, tie);
    }
}

void atem_parse_supersource(atem_state_t* state, const uint8_t* data, uint16_t length)
{
    if (length < 4) return;

    state->supersource_fill = atem_get_u16(data, 0);
    state->supersource_key = atem_get_u16(data, 2);

    ATEM_LOGV("SuperSource Fill=%d, Key=%d",
             state->supersource_fill, state->supersource_key);
}
