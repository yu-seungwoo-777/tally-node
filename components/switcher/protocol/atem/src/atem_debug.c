/**
 * ATEM 디버그 시스템 구현
 *
 * ATEM 전용 패킷 덤프 함수
 * ESP-IDF esp_log 사용
 */

#include "atem_debug.h"
#include "atem_protocol.h"
#include "atem_client.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * ATEM 전용 패킷 덤프
 * ============================================================================ */

void atem_debug_dump_packet(const char* direction, const uint8_t* data, uint16_t length)
{
#if ATEM_DEBUG_PACKET
    printf("[ATEM:%s] %d bytes:\n", direction, length);

    /* 16바이트씩 출력 */
    for (uint16_t i = 0; i < length; i += 16) {
        printf("%04X: ", i);
        for (uint16_t j = 0; j < 16; j++) {
            if (i + j < length) {
                printf("%02X ", data[i + j]);
            } else {
                printf("   ");
            }
            if (j == 7) printf(" ");
        }
        printf(" |");
        for (uint16_t j = 0; j < 16 && i + j < length; j++) {
            uint8_t c = data[i + j];
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf("|\n");
    }
#else
    (void)direction;
    (void)data;
    (void)length;
#endif
}

void atem_debug_dump_command(const char* cmd_name, const uint8_t* data, uint16_t length)
{
#if ATEM_DEBUG_PACKET
    printf("[ATEM:CMD] %.4s (%d bytes):", cmd_name, length);
    uint16_t max_len = (length > 32) ? 32 : length;
    for (uint16_t i = 0; i < max_len; i++) {
        printf(" %02X", data[i]);
    }
    if (length > 32) printf(" ...");
    printf("\n");
#else
    (void)cmd_name;
    (void)data;
    (void)length;
#endif
}

void atem_debug_dump_header(const uint8_t* data)
{
#if ATEM_DEBUG_PACKET
    uint16_t header_word = ((uint16_t)data[0] << 8) | data[1];
    uint8_t flags = (header_word >> 11) & 0x1F;
    uint16_t length = header_word & 0x07FF;
    uint16_t session_id = ((uint16_t)data[2] << 8) | data[3];
    uint16_t ack_id = ((uint16_t)data[4] << 8) | data[5];
    uint16_t packet_id = ((uint16_t)data[10] << 8) | data[11];

    char flag_buf[32];
    atem_debug_flags_str(flags, flag_buf);

    printf("[ATEM:HDR] flags=0x%02X(%s) len=%d session=0x%04X ack=%d pkt=%d\n",
           flags, flag_buf, length, session_id, ack_id, packet_id);
#else
    (void)data;
#endif
}

/* ============================================================================
 * 유틸리티
 * ============================================================================ */

const char* atem_debug_flags_str(uint8_t flags, char* buf)
{
    buf[0] = '\0';

    if (flags & ATEM_FLAG_ACK_REQUEST) {
        strcat(buf, "REQ|");
    }
    if (flags & ATEM_FLAG_HELLO) {
        strcat(buf, "HELLO|");
    }
    if (flags & ATEM_FLAG_RESEND) {
        strcat(buf, "RESEND|");
    }
    if (flags & ATEM_FLAG_REQUEST_RESEND) {
        strcat(buf, "REQRS|");
    }
    if (flags & ATEM_FLAG_ACK) {
        strcat(buf, "ACK|");
    }

    /* 마지막 | 제거 */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '|') {
        buf[len - 1] = '\0';
    }

    if (buf[0] == '\0') {
        strcpy(buf, "NONE");
    }

    return buf;
}

/* ============================================================================
 * 상태 출력
 * ============================================================================ */

void atem_debug_print_topology(const void* client_ptr)
{
    const atem_client_t* client = (const atem_client_t*)client_ptr;
    if (!client) return;

    const atem_state_t* state = &client->state;

    printf("\n");
    printf("──────────────────────────────────────────\n");
    printf(" ATEM Topology\n");
    printf("──────────────────────────────────────────\n");
    printf(" Product    : %s\n", state->product_name[0] ? state->product_name : "(unknown)");
    printf(" Protocol   : %d.%d\n", state->protocol_major, state->protocol_minor);
    printf(" ME         : %d\n", state->num_mes);
    printf(" Sources    : %d\n", state->num_sources);
    printf(" Cameras    : %d\n", state->num_cameras);
    printf(" DSK        : %d\n", state->num_dsks);

    /* ME별 USK 수 출력 (1-based 표시) */
    for (uint8_t me = 0; me < state->num_mes && me < ATEM_MAX_MES; me++) {
        printf(" USK (ME%d)  : %d\n", me + 1, state->num_keyers[me]);
    }

    /* SuperSource */
    if (state->supersource_fill != 0 || state->supersource_key != 0) {
        printf(" SuperSrc   : Fill=%d, Key=%d\n",
               state->supersource_fill, state->supersource_key);
    } else {
        printf(" SuperSrc   : 없음\n");
    }

    printf("──────────────────────────────────────────\n");

    /* Input 목록 출력 */
    if (state->input_count > 0) {
        printf("\n");
        printf(" Inputs (%d):\n", state->input_count);
        printf("──────────────────────────────────────────\n");
        for (uint8_t i = 0; i < state->input_count; i++) {
            const atem_input_info_t* input = &state->inputs[i];
            if (input->valid) {
                printf(" %4d : %-20s (%s)\n",
                       input->source_id, input->long_name, input->short_name);
            }
        }
        printf("──────────────────────────────────────────\n");
    }

    printf("\n");
    fflush(stdout);
}
